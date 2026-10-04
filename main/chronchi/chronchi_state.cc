#include "chronchi_state.h"

#include <esp_timer.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace {
constexpr char kDefaultTime[] = "--:--";
}

ChronchiState::ChronchiState() {
    home_.type = ChronchiScreenType::Home;
    CopyText(home_.source_app, sizeof(home_.source_app), "CHRONCHI");
    CopyText(home_.time, sizeof(home_.time), kDefaultTime);
    CopyText(home_.date, sizeof(home_.date), "Menunggu sinkronisasi");
    CopyText(home_.weather, sizeof(home_.weather), "-- C");
    CopyText(home_.location, sizeof(home_.location), "Android belum terhubung");
    CopyText(home_.network, sizeof(home_.network), "BLE");
    active_ = home_;
}

void ChronchiState::CopyText(char* destination, size_t capacity, const char* source) {
    if (destination == nullptr || capacity == 0) return;
    destination[0] = '\0';
    if (source == nullptr) return;

    size_t output = 0;
    size_t input = 0;
    const size_t length = std::strlen(source);
    while (input < length && output + 1 < capacity) {
        const uint8_t c = static_cast<uint8_t>(source[input]);
        if (c < 0x80) {
            destination[output++] = (c == '\r' || c == '\n' || c < 0x20)
                                        ? ' ' : static_cast<char>(c);
            ++input;
            continue;
        }

        size_t sequence = 0;
        if (c >= 0xc2 && c <= 0xdf) sequence = 2;
        else if (c >= 0xe0 && c <= 0xef) sequence = 3;
        else if (c >= 0xf0 && c <= 0xf4) sequence = 4;
        bool valid = sequence != 0 && input + sequence <= length;
        for (size_t i = 1; valid && i < sequence; ++i) {
            valid = (static_cast<uint8_t>(source[input + i]) & 0xc0) == 0x80;
        }
        if (valid && sequence == 3) {
            const uint8_t second = static_cast<uint8_t>(source[input + 1]);
            valid = !((c == 0xe0 && second < 0xa0) || (c == 0xed && second >= 0xa0));
        } else if (valid && sequence == 4) {
            const uint8_t second = static_cast<uint8_t>(source[input + 1]);
            valid = !((c == 0xf0 && second < 0x90) || (c == 0xf4 && second >= 0x90));
        }
        if (!valid) {
            destination[output++] = '?';
            ++input;
            continue;
        }
        if (output + sequence >= capacity) break;
        std::memcpy(destination + output, source + input, sequence);
        output += sequence;
        input += sequence;
    }
    destination[output] = '\0';
}

uint8_t ChronchiState::PriorityFor(ChronchiScreenType type) {
    switch (type) {
        case ChronchiScreenType::Navigation: return 100;
        case ChronchiScreenType::Payment: return 80;
        case ChronchiScreenType::Message:
        case ChronchiScreenType::Professional: return 70;
        case ChronchiScreenType::Order: return 60;
        case ChronchiScreenType::Connection:
        case ChronchiScreenType::System: return 50;
        case ChronchiScreenType::Startup:
        case ChronchiScreenType::Home: return 0;
    }
    return 0;
}

uint32_t ChronchiState::TimeoutFor(ChronchiScreenType type) {
    switch (type) {
        case ChronchiScreenType::Message:
        case ChronchiScreenType::Professional:
        case ChronchiScreenType::Order: return 7000;
        case ChronchiScreenType::Payment: return 8000;
        case ChronchiScreenType::Connection:
        case ChronchiScreenType::System: return 5000;
        case ChronchiScreenType::Startup: return 1500;
        case ChronchiScreenType::Navigation:
        case ChronchiScreenType::Home: return 0;
    }
    return 0;
}

void ChronchiState::SetActiveLocked(const ChronchiScreen& screen, int64_t now_us) {
    active_ = screen;
    StampDeviceBatteryLocked(active_);
    const uint32_t timeout_ms = TimeoutFor(screen.type);
    expires_us_ = timeout_ms == 0 ? 0 : now_us + static_cast<int64_t>(timeout_ms) * 1000;
    ++revision_;
}

void ChronchiState::StampDeviceBatteryLocked(ChronchiScreen& screen) const {
    if (device_battery_valid_) {
        screen.battery = device_battery_;
        screen.charging = device_charging_;
        screen.battery_valid = true;
    } else if (phone_battery_valid_) {
        screen.battery = phone_battery_;
        screen.charging = phone_charging_;
        screen.battery_valid = true;
    } else {
        screen.battery = 0;
        screen.charging = false;
        screen.battery_valid = false;
    }
    screen.connection = connected_ ? ChronchiConnectionState::Connected
                                   : ChronchiConnectionState::Disconnected;
}

bool ChronchiState::ActiveExpiredLocked(int64_t now_us) const {
    return expires_us_ != 0 && now_us >= expires_us_;
}

void ChronchiState::ShowStartup() {
    ChronchiScreen screen = {};
    screen.type = ChronchiScreenType::Startup;
    CopyText(screen.source_app, sizeof(screen.source_app), "CHRONCHI");
    CopyText(screen.primary, sizeof(screen.primary), "Starting...");
    CopyText(screen.secondary, sizeof(screen.secondary), "ESP BRIDGE");
    std::lock_guard<std::mutex> lock(mutex_);
    SetActiveLocked(screen, esp_timer_get_time());
}

void ChronchiState::SetConnection(bool connected) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connected_ == connected) return;
    connected_ = connected;
    if (!connected) subscribed_ = false;

    home_.connection = connected ? ChronchiConnectionState::Connected
                                 : ChronchiConnectionState::Disconnected;
    active_.connection = home_.connection;

    ChronchiScreen screen = {};
    screen.type = ChronchiScreenType::Connection;
    screen.connection = home_.connection;
    CopyText(screen.source_app, sizeof(screen.source_app), "BLUETOOTH");
    CopyText(screen.primary, sizeof(screen.primary), connected ? "Connected" : "Disconnected");
    CopyText(screen.secondary, sizeof(screen.secondary),
             connected ? "Chronchi Phone" : "Reconnecting...");
    const int64_t now = esp_timer_get_time();
    if (!navigation_active_ &&
        (ActiveExpiredLocked(now) || PriorityFor(screen.type) >= PriorityFor(active_.type))) {
        SetActiveLocked(screen, now);
    } else {
        ++revision_;
    }
}

void ChronchiState::SetSubscribed(bool subscribed) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (subscribed_ == subscribed) return;
    subscribed_ = subscribed;
    ++revision_;
}

void ChronchiState::RefreshHomeLocked() {
    StampDeviceBatteryLocked(home_);
    if (!navigation_active_ && active_.type == ChronchiScreenType::Home) {
        active_ = home_;
    }
    ++revision_;
}

void ChronchiState::UpdateClock(const char* time, const char* date) {
    std::lock_guard<std::mutex> lock(mutex_);
    CopyText(home_.time, sizeof(home_.time), time);
    CopyText(home_.date, sizeof(home_.date), date);
    RefreshHomeLocked();
}

void ChronchiState::Set24HourMode(bool hour24) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (hour24_ == hour24) return;
    hour24_ = hour24;
    ++revision_;
}

void ChronchiState::UpdatePhoneStatus(uint8_t battery, bool charging) {
    std::lock_guard<std::mutex> lock(mutex_);
    phone_battery_ = std::min<uint8_t>(battery, 100);
    phone_charging_ = charging;
    phone_battery_valid_ = true;
    if (!device_battery_valid_) {
        StampDeviceBatteryLocked(home_);
        StampDeviceBatteryLocked(active_);
        ++revision_;
    }
}

void ChronchiState::UpdateWeatherHighLow(const char* high_low) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (high_low != nullptr) {
        CopyText(weather_high_low_, sizeof(weather_high_low_), high_low);
    }
}

void ChronchiState::UpdateDeviceBattery(uint8_t battery, bool charging) {
    std::lock_guard<std::mutex> lock(mutex_);
    const uint8_t normalized = std::min<uint8_t>(battery, 100);
    if (device_battery_valid_ && device_battery_ == normalized &&
        device_charging_ == charging) {
        return;
    }
    device_battery_ = normalized;
    device_charging_ = charging;
    device_battery_valid_ = true;
    StampDeviceBatteryLocked(home_);
    StampDeviceBatteryLocked(active_);
    ++revision_;
}

void ChronchiState::UpdateNetwork(const char* network, uint8_t signal, bool wifi) {
    std::lock_guard<std::mutex> lock(mutex_);
    CopyText(home_.network, sizeof(home_.network), network);
    home_.signal = std::min<uint8_t>(signal, 4);
    home_.wifi = wifi;
    RefreshHomeLocked();
}

void ChronchiState::UpdateWeather(const char* weather, const char* location) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (weather != nullptr && weather[0] != '\0') {
        CopyText(home_.weather, sizeof(home_.weather), weather);
    }
    if (location != nullptr && location[0] != '\0') {
        CopyText(home_.location, sizeof(home_.location), location);
    }
    RefreshHomeLocked();
}

void ChronchiState::ApplyScreen(const ChronchiScreen& screen) {
    std::lock_guard<std::mutex> lock(mutex_);
    const int64_t now = esp_timer_get_time();
    if (screen.type == ChronchiScreenType::Home) {
        home_ = screen;
        StampDeviceBatteryLocked(home_);
        if (!navigation_active_ &&
            (active_.type == ChronchiScreenType::Home || ActiveExpiredLocked(now))) {
            SetActiveLocked(home_, now);
        } else {
            ++revision_;
        }
        return;
    }

    if (screen.type == ChronchiScreenType::Navigation) {
        navigation_active_ = true;
        last_navigation_us_ = now;
        saved_navigation_ = screen;
        SetActiveLocked(screen, now);
        return;
    }

    if (screen.type == ChronchiScreenType::Message ||
        screen.type == ChronchiScreenType::Professional ||
        screen.type == ChronchiScreenType::Payment ||
        screen.type == ChronchiScreenType::Order) {
        home_.notification_count = std::min<uint8_t>(
            static_cast<uint8_t>(home_.notification_count + 1), 99);
    }

    if (navigation_active_) {
        // Display transient notification for its normal duration over navigation
        SetActiveLocked(screen, now);
        return;
    }

    if (ActiveExpiredLocked(now) || PriorityFor(screen.type) >= PriorityFor(active_.type)) {
        SetActiveLocked(screen, now);
    } else {
        // The event is accepted but cannot steal a higher-priority screen.
        ++revision_;
    }
}

void ChronchiState::UpdateNavCustomIcon(const uint8_t* icon_32x32) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!navigation_active_ || icon_32x32 == nullptr) return;
    saved_navigation_.has_custom_icon = true;
    std::memcpy(saved_navigation_.custom_icon, icon_32x32, sizeof(saved_navigation_.custom_icon));
    if (active_.type == ChronchiScreenType::Navigation) {
        active_.has_custom_icon = true;
        std::memcpy(active_.custom_icon, icon_32x32, sizeof(active_.custom_icon));
        ++revision_;
    }
}

void ChronchiState::StopNavigation() {
    std::lock_guard<std::mutex> lock(mutex_);
    navigation_active_ = false;
    last_navigation_us_ = 0;
    saved_navigation_ = {};
    SetActiveLocked(home_, esp_timer_get_time());
}

void ChronchiState::DismissTransient() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (active_.type == ChronchiScreenType::Home) return;
    if (navigation_active_) {
        SetActiveLocked(saved_navigation_, esp_timer_get_time());
    } else {
        SetActiveLocked(home_, esp_timer_get_time());
    }
}

void ChronchiState::ShowSystem(const char* primary, const char* secondary) {
    ChronchiScreen screen = {};
    screen.type = ChronchiScreenType::System;
    CopyText(screen.source_app, sizeof(screen.source_app), "SYSTEM");
    CopyText(screen.primary, sizeof(screen.primary), primary);
    CopyText(screen.secondary, sizeof(screen.secondary), secondary);
    ApplyScreen(screen);
}

bool ChronchiState::Tick() {
    std::lock_guard<std::mutex> lock(mutex_);
    const int64_t now = esp_timer_get_time();
    bool changed = false;

    // Real-time clock update from ESP32 RTC
    const time_t now_sec = time(nullptr);
    if (now_sec > 1700000000) {  // Valid RTC time (year >= 2024)
        struct tm tm_val = {};
        localtime_r(&now_sec, &tm_val);

        char current_time[9] = {};
        if (hour24_) {
            std::snprintf(current_time, sizeof(current_time), "%02d:%02d",
                          tm_val.tm_hour, tm_val.tm_min);
        } else {
            int h12 = tm_val.tm_hour % 12;
            if (h12 == 0) h12 = 12;
            std::snprintf(current_time, sizeof(current_time), "%02d:%02d",
                          h12, tm_val.tm_min);
        }

        static const char* const kDayNames[] = {
            "Min", "Sen", "Sel", "Rab", "Kam", "Jum", "Sab"
        };
        const char* day_name = (tm_val.tm_wday >= 0 && tm_val.tm_wday < 7)
                                   ? kDayNames[tm_val.tm_wday]
                                   : "";
        char current_date[33] = {};
        std::snprintf(current_date, sizeof(current_date), "%s, %02d/%02d/%04d",
                      day_name, tm_val.tm_mday, tm_val.tm_mon + 1, tm_val.tm_year + 1900);

        if (std::strcmp(home_.time, current_time) != 0) {
            CopyText(home_.time, sizeof(home_.time), current_time);
            CopyText(home_.date, sizeof(home_.date), current_date);
            if (active_.type == ChronchiScreenType::Home) {
                CopyText(active_.time, sizeof(active_.time), current_time);
                CopyText(active_.date, sizeof(active_.date), current_date);
            }
            changed = true;
        }
    }

    // Auto-revert navigation if no update received for 60 seconds
    if (navigation_active_ && last_navigation_us_ != 0 &&
        (now - last_navigation_us_ > 60 * 1000 * 1000LL)) {
        navigation_active_ = false;
        last_navigation_us_ = 0;
        saved_navigation_ = {};
        SetActiveLocked(home_, now);
        return true;
    }

    if (ActiveExpiredLocked(now)) {
        if (navigation_active_) {
            SetActiveLocked(saved_navigation_, now);
        } else {
            SetActiveLocked(home_, now);
        }
        return true;
    }

    if (changed) {
        ++revision_;
        return true;
    }
    return false;
}

ChronchiSnapshot ChronchiState::Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    ChronchiSnapshot snapshot = {};
    snapshot.revision = revision_;
    snapshot.connected = connected_;
    snapshot.subscribed = subscribed_;
    snapshot.navigation_active = navigation_active_;
    snapshot.active = active_;
    return snapshot;
}
