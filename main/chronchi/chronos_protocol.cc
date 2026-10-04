#include "chronchi_alert.h"
#include "chronos_protocol.h"

#include <esp_log.h>
#include <esp_system.h>
#include <sys/time.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace {
constexpr char kTag[] = "ChronosProto";

void ExtractNullTerminated(char* dest, size_t dest_size, const uint8_t* data, size_t total_len, size_t& offset) {
    if (dest == nullptr || dest_size == 0) return;
    dest[0] = '\0';
    size_t i = 0;
    while (offset < total_len && data[offset] != 0) {
        if (i + 1 < dest_size) {
            dest[i++] = static_cast<char>(data[offset]);
        }
        offset++;
    }
    dest[i] = '\0';
    if (offset < total_len && data[offset] == 0) {
        offset++;  // Skip the null terminator
    }
}
}  // namespace

ChronosProtocol::ChronosProtocol(ChronchiState& state)
    : state_(state) {}

void ChronosProtocol::Reset() {
    expected_length_ = 0;
    current_length_ = 0;
    std::memset(buffer_, 0, sizeof(buffer_));
}

bool ChronosProtocol::FeedFrame(const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0) return false;

    // Detect first chunk of packet
    if ((data[0] == 0xAB || data[0] == 0xEA) && length >= 4 && (data[3] == 0xFE || data[3] == 0xFF)) {
        expected_length_ = static_cast<size_t>(data[1]) * 256 + data[2] + 3;
        current_length_ = std::min(length, sizeof(buffer_));
        std::memcpy(buffer_, data, current_length_);

        if (current_length_ >= expected_length_) {
            HandlePacket(buffer_, expected_length_);
            Reset();
            return true;
        }
        return false;
    }

    // Follow-up chunk of multi-frame transfer
    if (expected_length_ > current_length_ && length > 1) {
        const size_t offset = 20 + static_cast<size_t>(data[0]) * 19;
        const size_t payload_len = length - 1;
        if (offset + payload_len <= sizeof(buffer_)) {
            std::memcpy(buffer_ + offset, data + 1, payload_len);
            current_length_ = std::max(current_length_, offset + payload_len);
            if (current_length_ >= expected_length_) {
                HandlePacket(buffer_, expected_length_);
                Reset();
                return true;
            }
        } else {
            ESP_LOGW(kTag, "Chunk buffer overflow: offset=%u len=%u", static_cast<unsigned>(offset), static_cast<unsigned>(payload_len));
            Reset();
        }
    }
    return false;
}

void ChronosProtocol::HandlePacket(const uint8_t* data, size_t length) {
    if (data == nullptr || length < 5) return;

    if (data[0] == 0xAB) {
        const uint8_t command = data[4];
        switch (command) {
            case 0x93:  // Time sync
                HandleTimeSync(data, length);
                break;
            case 0x91:  // Phone battery
                HandlePhoneBattery(data, length);
                break;
            case 0x7E:  // Weather
                HandleWeather(data, length);
                break;
            case 0x88:  // Weather high / low temperature
                if (length >= 8) {
                    const int signH = (data[6] & 0x80) ? -1 : 1;
                    const int tempH = static_cast<int>(data[6] & 0x7F) * signH;
                    const int signL = (data[7] & 0x80) ? -1 : 1;
                    const int tempL = static_cast<int>(data[7] & 0x7F) * signL;
                    char hl_str[24] = {};
                    std::snprintf(hl_str, sizeof(hl_str), "%d/%d C", tempL, tempH);
                    state_.UpdateWeatherHighLow(hl_str);
                    ESP_LOGI(kTag, "Chronos weather high/low: %s", hl_str);
                }
                break;
            case 0x72:  // Notification / Ringer
                HandleNotification(data, length);
                break;
            case 0xEF:  // Navigation
                HandleNavigation(data, length);
                break;
            case 0xEE:  // Navigation Icon data
                HandleNavigationIcon(data, length);
                break;
            case 0x71:  // Find Watch
                state_.ShowSystem("CARI PERANGKAT", "Panggilan dari Chronos");
                ChronchiAlert::GetInstance().StartFindDeviceAlarm();
                break;
            case 0x79:  // Camera state
                if (length >= 7) {
                    camera_ready_ = (data[6] == 1);
                    if (camera_ready_) {
                        state_.ShowSystem("KAMERA AKTIF", "Tekan tombol untuk foto");
                    } else {
                        state_.DismissTransient();
                    }
                    ESP_LOGI(kTag, "Chronos camera ready: %s", camera_ready_ ? "yes" : "no");
                }
                break;
            case 0x7C:  // 24-hour mode
                if (length >= 7) {
                    state_.Set24HourMode(data[6] == 0);
                    ESP_LOGI(kTag, "Chronos 24-hour mode set to: %s", (data[6] == 0) ? "24h" : "12h");
                }
                break;
            case 0x9D:  // Music info
                HandleMusic(data, length);
                break;
            case 0xCA:  // App version / Watch Info request
                send_esp_requested_ = true;
                ESP_LOGI(kTag, "Chronos requested watch specs (0xCA)");
                break;
            case 0x23:  // Reset command
                ESP_LOGI(kTag, "Chronos reset command received");
                break;
            default:
                ESP_LOGD(kTag, "Unhandled Chronos cmd 0x%02X len %u", command, static_cast<unsigned>(length));
                break;
        }
    } else if (data[0] == 0xEA) {
        const uint8_t command = data[4];
        if (command == 0x7E && length >= 7 && data[5] == 0x01) {
            // Weather city name
            char city[32] = {};
            const size_t city_len = std::min(length - 7, sizeof(city) - 1);
            std::memcpy(city, data + 7, city_len);
            city[city_len] = '\0';
            state_.UpdateWeather(nullptr, city);
        } else if (command == 0x7F && length >= 15 && data[3] == 0xFE) {
            // Weather location payload: [6]=payloadLen, [7..14]=lat/lon, [15..]=city
            const uint8_t payload_len = data[6];
            if (length >= 7 + payload_len && payload_len > 8) {
                const uint8_t* payload = data + 7;
                char city[32] = {};
                size_t c_idx = 8;
                size_t out_idx = 0;
                while (c_idx < payload_len && payload[c_idx] != 0 && out_idx + 1 < sizeof(city)) {
                    city[out_idx++] = static_cast<char>(payload[c_idx++]);
                }
                city[out_idx] = '\0';
                if (city[0] != '\0') {
                    state_.UpdateWeather(nullptr, city);
                    ESP_LOGI(kTag, "Chronos weather location city: %s", city);
                }
            }
        }
    }
}

void ChronosProtocol::HandleTimeSync(const uint8_t* data, size_t length) {
    if (length < 14) return;
    const uint16_t year = static_cast<uint16_t>(data[7]) * 256 + data[8];
    const uint8_t month = data[9];
    const uint8_t day = data[10];
    const uint8_t hour = data[11];
    const uint8_t minute = data[12];
    const uint8_t second = data[13];

    if (year < 2020 || year > 2099 || month < 1 || month > 12 ||
        day < 1 || day > 31 || hour > 23 || minute > 59 || second > 59) {
        ESP_LOGW(kTag, "Invalid time sync: %04u-%02u-%02u %02u:%02u:%02u",
                 year, month, day, hour, minute, second);
        return;
    }

    struct tm tm_val = {};
    tm_val.tm_year = year - 1900;
    tm_val.tm_mon = month - 1;
    tm_val.tm_mday = day;
    tm_val.tm_hour = hour;
    tm_val.tm_min = minute;
    tm_val.tm_sec = second;
    const time_t timestamp = mktime(&tm_val);
    if (timestamp != static_cast<time_t>(-1)) {
        const struct timeval tv = { .tv_sec = timestamp, .tv_usec = 0 };
        settimeofday(&tv, nullptr);
    }

    char time_str[9] = {};
    char date_str[33] = {};
    std::snprintf(time_str, sizeof(time_str), "%02u:%02u", hour, minute);
    std::snprintf(date_str, sizeof(date_str), "%02u/%02u/%04u", day, month, year);
    state_.UpdateClock(time_str, date_str);
    ESP_LOGI(kTag, "Chronos time synced: %s %s", time_str, date_str);
}

void ChronosProtocol::HandlePhoneBattery(const uint8_t* data, size_t length) {
    if (length < 8) return;
    const bool charging = (data[6] == 1);
    const uint8_t level = std::min<uint8_t>(data[7], 100);
    state_.UpdatePhoneStatus(level, charging);
    ESP_LOGI(kTag, "Chronos phone battery: %u%% (charging: %s)", level, charging ? "yes" : "no");
}

void ChronosProtocol::HandleWeather(const uint8_t* data, size_t length) {
    if (length < 8) return;
    const uint8_t icon = data[6] >> 4;
    const int sign = (data[6] & 1) ? -1 : 1;
    const int temp = static_cast<int8_t>(data[7]) * sign;

    const char* cond_str = "Cerah";
    switch (icon) {
        case 0: cond_str = "Berawan"; break;
        case 1: cond_str = "Cerah"; break;
        case 2: cond_str = "Salju"; break;
        case 3: cond_str = "Hujan"; break;
        case 4: cond_str = "Mendung"; break;
        case 5: cond_str = "Badai"; break;
        case 6: cond_str = "Angin"; break;
        case 7: cond_str = "Kabut"; break;
        default: cond_str = "Cerah"; break;
    }

    char weather_str[17] = {};
    std::snprintf(weather_str, sizeof(weather_str), "%d C %s", temp, cond_str);
    state_.UpdateWeather(weather_str, "Chronos");
    ESP_LOGI(kTag, "Chronos weather updated: %s", weather_str);
}

void ChronosProtocol::HandleNotification(const uint8_t* data, size_t length) {
    if (length < 8) return;
    const uint8_t icon = data[6];
    const uint8_t state = data[7];

    if (icon == 0x01) {
        char caller[48] = {};
        if (length > 8) {
            const size_t caller_len = std::min(length - 8, sizeof(caller) - 1);
            std::memcpy(caller, data + 8, caller_len);
            caller[caller_len] = '\0';
        }
        state_.ShowSystem("PANGGILAN", caller[0] != '\0' ? caller : "Panggilan Masuk");
        ChronchiAlert::GetInstance().StartCallAlert();
        return;
    }
    if (icon == 0x02 || state == 0x03) {
        state_.DismissTransient();
        ChronchiAlert::GetInstance().StopAlert();
        return;
    }

    if (length <= 8) return;

    char message[256] = {};
    const size_t text_len = std::min(length - 8, sizeof(message) - 1);
    std::memcpy(message, data + 8, text_len);
    message[text_len] = '\0';

    const char* app_name = AppNameForIcon(icon);

    char title[65] = {};
    char body[129] = {};

    // Check for delimiter like ':' or '\n' in message
    const char* colon = std::strchr(message, ':');
    const char* newline = std::strchr(message, '\n');
    const char* sep = colon;
    if (sep == nullptr || (newline != nullptr && newline < sep)) {
        sep = newline;
    }

    if (sep != nullptr && (sep - message) < 32) {
        const size_t title_len = std::min<size_t>(static_cast<size_t>(sep - message), sizeof(title) - 1);
        std::memcpy(title, message, title_len);
        title[title_len] = '\0';

        const char* body_start = sep + 1;
        while (*body_start == ' ' || *body_start == '\r' || *body_start == '\n') {
            body_start++;
        }
        ChronchiState::CopyText(body, sizeof(body), body_start);
    } else {
        ChronchiState::CopyText(title, sizeof(title), app_name);
        ChronchiState::CopyText(body, sizeof(body), message);
    }

    ChronchiScreen screen = {};
    screen.type = ChronchiScreenType::Message;
    ChronchiState::CopyText(screen.source_app, sizeof(screen.source_app), app_name);
    ChronchiState::CopyText(screen.primary, sizeof(screen.primary), title);
    ChronchiState::CopyText(screen.secondary, sizeof(screen.secondary), body);
    state_.ApplyScreen(screen);
    ChronchiAlert::GetInstance().VibrateNotification();
    ESP_LOGI(kTag, "Chronos notification: [%s] %s: %s", app_name, title, body);
}

void ChronosProtocol::HandleNavigation(const uint8_t* data, size_t length) {
    if (length < 6) return;
    const uint8_t mode = data[5];

    if (mode == 0x00) {
        ESP_LOGI(kTag, "Chronos navigation stopped");
        nav_icon_pos_mask_ = 0;
        nav_icon_valid_ = false;
        state_.StopNavigation();
        return;
    }

    if (((mode & 0x80) != 0 || mode == 0x01) && length >= 12) {
        const bool has_icon = (data[6] == 1);
        char title[64] = {};
        char duration[32] = {};
        char distance[32] = {};
        char eta[32] = {};
        char directions[96] = {};
        char speed[32] = {};

        size_t offset = 12;
        ExtractNullTerminated(title, sizeof(title), data, length, offset);
        ExtractNullTerminated(duration, sizeof(duration), data, length, offset);
        ExtractNullTerminated(distance, sizeof(distance), data, length, offset);
        ExtractNullTerminated(eta, sizeof(eta), data, length, offset);
        ExtractNullTerminated(directions, sizeof(directions), data, length, offset);
        ExtractNullTerminated(speed, sizeof(speed), data, length, offset);

        ChronchiScreen screen = {};
        screen.type = ChronchiScreenType::Navigation;
        ChronchiState::CopyText(screen.source_app, sizeof(screen.source_app), "NAVIGASI");

        // Parse maneuver by inspecting BOTH directions and title
        screen.maneuver = ParseManeuverFromText(directions, title);

        // Arrange Primary (instruction / street), Secondary (distance / duration / street), Footer (ETA)
        if (title[0] != '\0' && directions[0] != '\0') {
            const ChronchiManeuver title_maneuver = ParseManeuverFromText(title);
            if (title_maneuver != ChronchiManeuver::Straight && title_maneuver != ChronchiManeuver::Unknown) {
                // Title has the action (e.g. "Belok Kanan")
                ChronchiState::CopyText(screen.primary, sizeof(screen.primary), title);
                ChronchiState::CopyText(screen.secondary, sizeof(screen.secondary), directions);
            } else {
                // Directions has the instruction / road
                ChronchiState::CopyText(screen.primary, sizeof(screen.primary), directions);
                if (distance[0] != '\0') {
                    if (duration[0] != '\0') {
                        char dist_dur[72] = {};
                        std::snprintf(dist_dur, sizeof(dist_dur), "%s - %s", distance, duration);
                        ChronchiState::CopyText(screen.secondary, sizeof(screen.secondary), dist_dur);
                    } else {
                        ChronchiState::CopyText(screen.secondary, sizeof(screen.secondary), distance);
                    }
                } else {
                    ChronchiState::CopyText(screen.secondary, sizeof(screen.secondary), title);
                }
            }
        } else {
            ChronchiState::CopyText(screen.primary, sizeof(screen.primary),
                                    directions[0] != '\0' ? directions : title);
            ChronchiState::CopyText(screen.secondary, sizeof(screen.secondary),
                                    distance[0] != '\0' ? distance : duration);
        }

        // Footer: ETA or duration
        if (eta[0] != '\0') {
            if (duration[0] != '\0' && screen.secondary[0] != '\0' && std::strstr(screen.secondary, duration) == nullptr) {
                char eta_dur[72] = {};
                std::snprintf(eta_dur, sizeof(eta_dur), "%s (%s)", eta, duration);
                ChronchiState::CopyText(screen.footer, sizeof(screen.footer), eta_dur);
            } else {
                ChronchiState::CopyText(screen.footer, sizeof(screen.footer), eta);
            }
        } else if (duration[0] != '\0' && distance[0] != '\0' && std::strstr(screen.secondary, duration) == nullptr) {
            ChronchiState::CopyText(screen.footer, sizeof(screen.footer), duration);
        }

        // Custom icon from Chronos if received
        if (has_icon && nav_icon_valid_) {
            screen.has_custom_icon = true;
            std::memcpy(screen.custom_icon, nav_icon_32x32_, sizeof(screen.custom_icon));
        }

        state_.ApplyScreen(screen);
        ESP_LOGI(kTag, "Chronos nav update: mnv=%u icon=%d dir='%s' dist='%s' eta='%s'",
                 static_cast<unsigned>(screen.maneuver), screen.has_custom_icon,
                 screen.primary, screen.secondary, screen.footer);
    }
}

void ChronosProtocol::HandleNavigationIcon(const uint8_t* data, size_t length) {
    if (data == nullptr || length < 107 || data[3] != 0xFE) return;
    const uint8_t pos = data[6];
    if (pos > 2) return;

    const uint32_t crc = (static_cast<uint32_t>(data[7]) << 24) |
                         (static_cast<uint32_t>(data[8]) << 16) |
                         (static_cast<uint32_t>(data[9]) << 8) |
                         static_cast<uint32_t>(data[10]);

    if (crc != nav_icon_crc_) {
        nav_icon_crc_ = crc;
        nav_icon_pos_mask_ = 0;
        nav_icon_valid_ = false;
        std::memset(nav_icon_48x48_, 0, sizeof(nav_icon_48x48_));
    }

    std::memcpy(nav_icon_48x48_ + (pos * 96), data + 11, 96);
    nav_icon_pos_mask_ |= static_cast<uint8_t>(1 << pos);

    ESP_LOGD(kTag, "Chronos nav icon chunk pos=%u crc=0x%08lX mask=0x%02X",
             pos, static_cast<unsigned long>(crc), nav_icon_pos_mask_);

    if (nav_icon_pos_mask_ == 0x07) {
        Downsample48To32(nav_icon_48x48_, nav_icon_32x32_);
        nav_icon_valid_ = true;
        state_.UpdateNavCustomIcon(nav_icon_32x32_);
        ESP_LOGI(kTag, "Chronos nav icon 48x48 assembled & applied to display");
    }
}

void ChronosProtocol::Downsample48To32(const uint8_t* src, uint8_t* dst) {
    if (src == nullptr || dst == nullptr) return;
    std::memset(dst, 0, 128);

    // Count set bits to check for inversion (normal is white arrow on black bg)
    int set_bits = 0;
    for (size_t i = 0; i < 288; ++i) {
        uint8_t b = src[i];
        while (b) {
            set_bits += (b & 1);
            b >>= 1;
        }
    }
    const bool invert = (set_bits > 1728);  // > 75% white indicates inverted image

    auto get_px = [src, invert](int x, int y) -> bool {
        if (x < 0 || x >= 48 || y < 0 || y >= 48) return false;
        const int byte_idx = y * 6 + (x / 8);
        const int bit_idx = 7 - (x % 8);
        bool val = (src[byte_idx] & (1 << bit_idx)) != 0;
        return invert ? !val : val;
    };

    for (int ty = 0; ty < 32; ++ty) {
        for (int tx = 0; tx < 32; ++tx) {
            const int sx = (tx * 3) / 2;
            const int sy = (ty * 3) / 2;
            // 2x2 area sampling ensures thin 1px lines are preserved
            const bool px = get_px(sx, sy) ||
                            (sx + 1 < 48 && get_px(sx + 1, sy)) ||
                            (sy + 1 < 48 && get_px(sx, sy + 1)) ||
                            (sx + 1 < 48 && sy + 1 < 48 && get_px(sx + 1, sy + 1));
            if (px) {
                dst[ty * 4 + (tx / 8)] |= static_cast<uint8_t>(0x80 >> (tx % 8));
            }
        }
    }
}

void ChronosProtocol::HandleMusic(const uint8_t* data, size_t length) {
    if (length < 7) return;
    const uint8_t sub = data[5];
    if (sub == 0x81 && length > 7) {
        // Title
        const size_t len = std::min(length - 7, sizeof(music_title_) - 1);
        std::memcpy(music_title_, data + 7, len);
        music_title_[len] = '\0';
    } else if (sub == 0x82 && length > 7) {
        // Artist
        const size_t len = std::min(length - 7, sizeof(music_artist_) - 1);
        std::memcpy(music_artist_, data + 7, len);
        music_artist_[len] = '\0';

        if (music_title_[0] != '\0') {
            ChronchiScreen screen = {};
            screen.type = ChronchiScreenType::Message;
            ChronchiState::CopyText(screen.source_app, sizeof(screen.source_app), "MUSIK");
            ChronchiState::CopyText(screen.primary, sizeof(screen.primary), music_title_);
            ChronchiState::CopyText(screen.secondary, sizeof(screen.secondary), music_artist_);
            state_.ApplyScreen(screen);
        }
    }
}

size_t ChronosProtocol::BuildInfoPacket(uint8_t* buffer, size_t capacity) {
    if (buffer == nullptr || capacity < 20) return 0;
    const uint8_t packet[20] = {
        0xAB, 0x00, 0x11, 0xFF, 0x92, 0xC0,
        1, 91, // version 1.9.1
        0x00, 0xFB, 0x1E, 0x40, 0xC0, 0x0E, 0x32, 0x28, 0x00, 0xE2,
        0x00, // screen configuration
        0x80
    };
    std::memcpy(buffer, packet, 20);
    return 20;
}

size_t ChronosProtocol::BuildBatteryPacket(uint8_t* buffer, size_t capacity, uint8_t level, bool charging) {
    if (buffer == nullptr || capacity < 8) return 0;
    buffer[0] = 0xAB;
    buffer[1] = 0x00;
    buffer[2] = 0x05;
    buffer[3] = 0xFF;
    buffer[4] = 0x91;
    buffer[5] = 0x80;
    buffer[6] = charging ? 0x01 : 0x00;
    buffer[7] = level > 100 ? 100 : level;
    return 8;
}

size_t ChronosProtocol::BuildEspPacket(uint8_t* buffer, size_t capacity) {
    char esp_info[256] = {};
    std::snprintf(esp_info, sizeof(esp_info),
                  "Chronchi Dual (ESPBridge+Chronos)\n"
                  "Chip: ESP32-C3 Mini\n"
                  "Flash: 4MB | RAM: %u KB\n"
                  "OLED: SSD1306 128x64",
                  static_cast<unsigned>(esp_get_free_heap_size() / 1024));
    const size_t text_len = std::strlen(esp_info);
    const size_t total_len = text_len + 6;
    if (buffer == nullptr || capacity < total_len) return 0;
    buffer[0] = 0xAB;
    buffer[1] = static_cast<uint8_t>((text_len + 3) >> 8);
    buffer[2] = static_cast<uint8_t>((text_len + 3) & 0xff);
    buffer[3] = 0xFE;
    buffer[4] = 0x92;
    buffer[5] = 0x80;
    std::memcpy(buffer + 6, esp_info, text_len);
    return total_len;
}

size_t ChronosProtocol::BuildNotifyBatteryPacket(uint8_t* buffer, size_t capacity, bool state) {
    if (buffer == nullptr || capacity < 7) return 0;
    buffer[0] = 0xAB;
    buffer[1] = 0x00;
    buffer[2] = 0x04;
    buffer[3] = 0xFE;
    buffer[4] = 0x91;
    buffer[5] = 0x80;
    buffer[6] = state ? 0x01 : 0x00;
    return 7;
}

size_t ChronosProtocol::BuildSyncRequestPacket(uint8_t* buffer, size_t capacity) {
    if (buffer == nullptr || capacity < 6) return 0;
    buffer[0] = 0xAB;
    buffer[1] = 0x00;
    buffer[2] = 0x03;
    buffer[3] = 0xFE;
    buffer[4] = 0x23;
    buffer[5] = 0x80;
    return 6;
}

size_t ChronosProtocol::BuildFindPhonePacket(uint8_t* buffer, size_t capacity, bool state) {
    if (buffer == nullptr || capacity < 7) return 0;
    buffer[0] = 0xAB;
    buffer[1] = 0x00;
    buffer[2] = 0x04;
    buffer[3] = 0xFF;
    buffer[4] = 0x7D;
    buffer[5] = 0x80;
    buffer[6] = state ? 0x01 : 0x00;
    return 7;
}

size_t ChronosProtocol::BuildCameraCapturePacket(uint8_t* buffer, size_t capacity) {
    if (buffer == nullptr || capacity < 7) return 0;
    buffer[0] = 0xAB;
    buffer[1] = 0x00;
    buffer[2] = 0x04;
    buffer[3] = 0xFF;
    buffer[4] = 0x79;
    buffer[5] = 0x80;
    buffer[6] = 0x01;
    return 7;
}

size_t ChronosProtocol::BuildMusicControlPacket(uint8_t* buffer, size_t capacity, uint16_t command) {
    if (buffer == nullptr || capacity < 7) return 0;
    buffer[0] = 0xAB;
    buffer[1] = 0x00;
    buffer[2] = 0x04;
    buffer[3] = 0xFF;
    buffer[4] = static_cast<uint8_t>(command >> 8);
    buffer[5] = 0x80;
    buffer[6] = static_cast<uint8_t>(command & 0xFF);
    return 7;
}

const char* ChronosProtocol::AppNameForIcon(uint8_t icon) {
    switch (icon) {
        case 0x01: return "Panggilan";
        case 0x02: return "Panggilan Tak Terjawab";
        case 0x03: return "SMS";
        case 0x04: return "Email";
        case 0x07: return "Tencent";
        case 0x08: return "Skype";
        case 0x09: return "WeChat";
        case 0x0A: return "WhatsApp";
        case 0x0B: return "Gmail";
        case 0x0E: return "Line";
        case 0x0F: return "Twitter";
        case 0x10: return "Facebook";
        case 0x11: return "Messenger";
        case 0x12: return "Instagram";
        case 0x13: return "Weibo";
        case 0x14: return "KakaoTalk";
        case 0x16: return "Viber";
        case 0x17: return "VKontakte";
        case 0x18: return "Telegram";
        case 0x1B: return "DingTalk";
        case 0x20: return "WhatsApp Biz";
        case 0xC0: return "Chronos";
        default: return "Notifikasi";
    }
}

ChronchiManeuver ChronosProtocol::ParseManeuverFromText(const char* text1, const char* text2) {
    char upper[256] = {};
    size_t out = 0;
    auto append = [&upper, &out](const char* src) {
        if (src == nullptr) return;
        while (*src != '\0' && out + 2 < sizeof(upper)) {
            upper[out++] = static_cast<char>(std::toupper(static_cast<unsigned char>(*src++)));
        }
        if (out + 1 < sizeof(upper)) {
            upper[out++] = ' ';
        }
    };
    append(text1);
    append(text2);
    upper[out] = '\0';

    if (out <= 1) return ChronchiManeuver::Straight;

    // 1. Arrive / Destination
    if (std::strstr(upper, "ARRIVE") || std::strstr(upper, "ARRIVED") ||
        std::strstr(upper, "DESTINATION") || std::strstr(upper, "REACHED") ||
        std::strstr(upper, "SAMPAI") || std::strstr(upper, "TIBA") ||
        std::strstr(upper, "TUJUAN")) {
        return ChronchiManeuver::Arrive;
    }

    // 2. Roundabout / U-turn
    if (std::strstr(upper, "ROUNDABOUT") || std::strstr(upper, "BUNDARAN") ||
        std::strstr(upper, "ROTARY") || std::strstr(upper, "CIRCLE") ||
        std::strstr(upper, "U-TURN") || std::strstr(upper, "UTURN") ||
        std::strstr(upper, "PUTAR BALIK") || std::strstr(upper, "BALIK ARAH") ||
        std::strstr(upper, "PUTARAN")) {
        return ChronchiManeuver::Roundabout;
    }

    // 3. Left vs Right
    const bool has_left = (std::strstr(upper, "LEFT") != nullptr) ||
                          (std::strstr(upper, "KIRI") != nullptr);
    const bool has_right = (std::strstr(upper, "RIGHT") != nullptr) ||
                           (std::strstr(upper, "KANAN") != nullptr);

    const bool has_slight = (std::strstr(upper, "SLIGHT") != nullptr) ||
                            (std::strstr(upper, "SERONG") != nullptr) ||
                            (std::strstr(upper, "AGAK") != nullptr) ||
                            (std::strstr(upper, "SEDIKIT") != nullptr) ||
                            (std::strstr(upper, "KEEP") != nullptr) ||
                            (std::strstr(upper, "TETAP") != nullptr) ||
                            (std::strstr(upper, "LAJUR") != nullptr) ||
                            (std::strstr(upper, "JALUR") != nullptr) ||
                            (std::strstr(upper, "AMBIL") != nullptr) ||
                            (std::strstr(upper, "BEAR") != nullptr) ||
                            (std::strstr(upper, "FORK") != nullptr) ||
                            (std::strstr(upper, "RAMP") != nullptr) ||
                            (std::strstr(upper, "MERGE") != nullptr);

    if (has_left && has_right) {
        const char* p_left = std::strstr(upper, "KIRI");
        if (!p_left) p_left = std::strstr(upper, "LEFT");

        const char* p_right = std::strstr(upper, "KANAN");
        if (!p_right) p_right = std::strstr(upper, "RIGHT");

        if (p_left < p_right) {
            return has_slight ? ChronchiManeuver::SlightLeft : ChronchiManeuver::Left;
        } else {
            return has_slight ? ChronchiManeuver::SlightRight : ChronchiManeuver::Right;
        }
    }

    if (has_left) {
        return has_slight ? ChronchiManeuver::SlightLeft : ChronchiManeuver::Left;
    }

    if (has_right) {
        return has_slight ? ChronchiManeuver::SlightRight : ChronchiManeuver::Right;
    }

    // 4. Straight
    if (std::strstr(upper, "STRAIGHT") || std::strstr(upper, "CONTINUE") ||
        std::strstr(upper, "HEAD") || std::strstr(upper, "LURUS") ||
        std::strstr(upper, "TERUS") || std::strstr(upper, "LANJUT")) {
        return ChronchiManeuver::Straight;
    }

    return ChronchiManeuver::Straight;
}
