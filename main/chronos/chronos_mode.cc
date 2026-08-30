#include "chronos_mode.h"

#include "boards/esp32c3-inmp441/config.h"
#include "boards/esp32c3-inmp441/shared_oled.h"
#include "display/display.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdio>

namespace {
constexpr char kTag[] = "ChronosMode";
constexpr uint8_t kPageCount = 4;

void LogHeap(const char* stage) {
    ESP_LOGI(kTag, "Heap %s: free=%u largest=%u min=%u",
             stage,
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)));
}
}  // namespace

ChronosMode::ChronosMode() : ble_(state_), button_(BOOT_BUTTON_GPIO) {}

void ChronosMode::Initialize() {
    ESP_LOGI(kTag, "Radio policy: Chronos owns BLE; Wi-Fi initialization is forbidden");
    ESP_LOGI(kTag, "Power policy: microphone, speaker, wake-word, and Xiaozhi tasks are not initialized");
    LogHeap("before OLED/BLE");

    display_ = GetEsp32c3SharedOled(true);
    mode_selector_ = std::make_unique<ModeSelector>(
        BootMode::Chronos, display_, [this]() { NextPage(); });
    button_.OnPressDown([this]() { mode_selector_->OnPressDown(); });
    button_.OnPressUp([this]() { mode_selector_->OnPressUp(); });

    Render(state_.Snapshot());
    ble_status_ = ble_.Initialize();
    if (ble_status_ != ESP_OK) {
        ESP_LOGE(kTag, "BLE unavailable: %s; mode button remains operational",
                 esp_err_to_name(ble_status_));
        display_->SetChronosPage("CHRONOS ERROR", "BLE init failed",
                                 esp_err_to_name(ble_status_), "hold 2.5s: mode", false);
    }
    LogHeap("after OLED/BLE");
}

void ChronosMode::NextPage() {
    uint8_t next = static_cast<uint8_t>((page_.load() + 1) % kPageCount);
    page_.store(next);
    Render(state_.Snapshot());
}

const char* ChronosMode::WeatherName(uint8_t icon) {
    switch (icon) {
        case 0: return "Sunny";
        case 1: return "Cloudy";
        case 2: return "Overcast";
        case 3: case 4: case 5: return "Rain";
        case 6: case 7: return "Storm";
        case 8: case 9: return "Snow";
        default: return "Weather";
    }
}

void ChronosMode::Render(const ChronosSnapshot& snapshot) {
    if (display_ == nullptr || mode_selector_ == nullptr || mode_selector_->IsMenuActive()) return;

    char title[64] = {};
    char line1[96] = {};
    char line2[128] = {};
    char footer[80] = {};
    const char* link = snapshot.subscribed ? "OK" : (snapshot.connected ? "WAIT" : "ADV");

    switch (page_.load()) {
        case 0: {
            std::snprintf(title, sizeof(title), "MOCHI  BLE:%s", link);
            if (snapshot.time_valid) {
                int64_t elapsed = (esp_timer_get_time() - snapshot.time_sync_us) / 1000000;
                int total = snapshot.hour * 3600 + snapshot.minute * 60 + snapshot.second +
                            static_cast<int>(elapsed);
                int hour = (total / 3600) % 24;
                int minute = (total / 60) % 60;
                char suffix[4] = {};
                if (!snapshot.use_24_hour) {
                    std::snprintf(suffix, sizeof(suffix), "%s", hour >= 12 ? "PM" : "AM");
                    hour %= 12;
                    if (hour == 0) hour = 12;
                }
                std::snprintf(footer, sizeof(footer), "%02d:%02d%s  %02u/%02u%s%u%%",
                              hour, minute, suffix, snapshot.day, snapshot.month,
                              snapshot.phone_charging ? " +" : "  P:",
                              snapshot.phone_battery_valid ? snapshot.phone_battery : 0);
            } else {
                std::snprintf(footer, sizeof(footer), "OPEN APP TO SYNC");
            }
            display_->SetChronosPage(title, "", "", footer, true);
            return;
        }
        case 1:
            std::snprintf(title, sizeof(title), "NOTIFY  BLE:%s", link);
            if (snapshot.notification_count == 0) {
                std::snprintf(line1, sizeof(line1), "NO NOTIFICATIONS");
                std::snprintf(line2, sizeof(line2), "WAITING FOR PHONE");
            } else {
                std::snprintf(line1, sizeof(line1), "%s", snapshot.notifications[0].title);
                std::snprintf(line2, sizeof(line2), "%.127s", snapshot.notifications[0].message);
            }
            std::snprintf(footer, sizeof(footer), "%u SAVED | CLICK NEXT", snapshot.notification_count);
            break;
        case 2:
            std::snprintf(title, sizeof(title), "WEATHER  BLE:%s", link);
            if (snapshot.weather_count == 0) {
                std::snprintf(line1, sizeof(line1), "NO WEATHER DATA");
                std::snprintf(line2, sizeof(line2), "SYNC IN CHRONOS APP");
            } else {
                std::snprintf(line1, sizeof(line1), "%s  %d C",
                              WeatherName(snapshot.weather[0].icon),
                              snapshot.weather[0].temperature);
                std::snprintf(line2, sizeof(line2), "H:%d  L:%d",
                              snapshot.weather[0].high, snapshot.weather[0].low);
            }
            std::snprintf(footer, sizeof(footer), "%s", snapshot.weather_city[0] ? snapshot.weather_city : "Location unknown");
            break;
        default:
            std::snprintf(title, sizeof(title), "NAV  BLE:%s", link);
            if (!snapshot.navigation.active) {
                std::snprintf(line1, sizeof(line1), "NAVIGATION INACTIVE");
                std::snprintf(line2, sizeof(line2), "START GOOGLE MAPS");
            } else {
                std::snprintf(line1, sizeof(line1), "%s  %s",
                              snapshot.navigation.distance, snapshot.navigation.duration);
                std::snprintf(line2, sizeof(line2), "%s", snapshot.navigation.directions);
            }
            std::snprintf(footer, sizeof(footer), "%s", snapshot.navigation.eta);
            break;
    }
    display_->SetChronosPage(title, line1, line2, footer, false);
}

[[noreturn]] void ChronosMode::Run() {
    uint32_t last_revision = UINT32_MAX;
    int64_t last_clock_second = -1;
    while (true) {
        if (ble_status_ == ESP_OK) ble_.Poll();
        ChronosSnapshot snapshot = state_.Snapshot();
        int64_t clock_second = esp_timer_get_time() / 1000000;
        if (snapshot.revision != last_revision ||
            (page_.load() == 0 && clock_second != last_clock_second)) {
            Render(snapshot);
            last_revision = snapshot.revision;
            last_clock_second = clock_second;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
