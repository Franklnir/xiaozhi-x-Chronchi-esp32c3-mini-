#include "chronos_state.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>

#include <algorithm>
#include <cstring>

namespace {
constexpr char kTag[] = "ChronosState";
}

void ChronosState::SetConnection(bool connected) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_.connected == connected) return;
    state_.connected = connected;
    if (!connected) state_.subscribed = false;
    ++state_.revision;
}

void ChronosState::SetSubscribed(bool subscribed) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_.subscribed == subscribed) return;
    state_.subscribed = subscribed;
    ++state_.revision;
}

ChronosSnapshot ChronosState::Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

void ChronosState::CopyText(char* destination, size_t capacity,
                            const uint8_t* source, size_t length) {
    if (capacity == 0) return;
    size_t output = 0;
    size_t i = 0;
    while (i < length && output + 1 < capacity) {
        const uint8_t c = source[i];
        if (c == 0) break;
        if (c < 0x80) {
            destination[output++] = (c == '\r' || c == '\n' || c < 0x20)
                                        ? ' ' : static_cast<char>(c);
            ++i;
            continue;
        }

        // Validate one complete UTF-8 scalar before copying it to LVGL. Invalid
        // and truncated byte sequences become '?' without reading out of range.
        size_t sequence = 0;
        if (c >= 0xc2 && c <= 0xdf) sequence = 2;
        else if (c >= 0xe0 && c <= 0xef) sequence = 3;
        else if (c >= 0xf0 && c <= 0xf4) sequence = 4;

        bool valid = sequence != 0 && i + sequence <= length;
        for (size_t j = 1; valid && j < sequence; ++j) {
            valid = (source[i + j] & 0xc0) == 0x80;
        }
        if (valid && sequence == 3) {
            valid = !((c == 0xe0 && source[i + 1] < 0xa0) ||
                      (c == 0xed && source[i + 1] >= 0xa0));
        } else if (valid && sequence == 4) {
            valid = !((c == 0xf0 && source[i + 1] < 0x90) ||
                      (c == 0xf4 && source[i + 1] >= 0x90));
        }
        if (!valid) {
            destination[output++] = '?';
            ++i;
            continue;
        }
        if (output + sequence >= capacity) break;
        std::memcpy(destination + output, source + i, sequence);
        output += sequence;
        i += sequence;
    }
    destination[output] = '\0';
}

bool ChronosState::ReadCString(const uint8_t* data, size_t length, size_t* cursor,
                               char* destination, size_t capacity) {
    if (*cursor >= length) {
        if (capacity) destination[0] = '\0';
        return false;
    }
    const size_t start = *cursor;
    while (*cursor < length && data[*cursor] != 0) ++(*cursor);
    CopyText(destination, capacity, data + start, *cursor - start);
    if (*cursor >= length) return false;
    ++(*cursor);
    return true;
}

bool ChronosState::ApplyFrame(const uint8_t* data, size_t length) {
    if (data == nullptr || length < 5 || (data[0] != 0xab && data[0] != 0xea)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const uint8_t opcode = data[4];

    if (data[0] == 0xea) {
        if (opcode == 0x7e && length >= 8 && data[5] == 0x01) {
            CopyText(state_.weather_city, sizeof(state_.weather_city), data + 7, length - 7);
            ++state_.revision;
            return true;
        }
        return false;
    }

    switch (opcode) {
        case 0x72: {
            if (length < 9 || data[7] != 0x02 || data[6] == 0x01 || data[6] == 0x02) {
                return false;
            }
            for (size_t i = state_.notifications.size() - 1; i > 0; --i) {
                state_.notifications[i] = state_.notifications[i - 1];
            }
            auto& notification = state_.notifications[0];
            notification = {};
            notification.icon = data[6];
            const uint8_t* message = data + 8;
            const size_t message_length = length - 8;
            size_t separator = message_length;
            for (size_t i = 0; i < message_length && i < 30; ++i) {
                if (message[i] == ':') {
                    separator = i;
                    break;
                }
            }
            if (separator < message_length) {
                CopyText(notification.title, sizeof(notification.title), message, separator);
                size_t body = separator + 1;
                while (body < message_length && message[body] == ' ') ++body;
                CopyText(notification.message, sizeof(notification.message),
                         message + body, message_length - body);
            } else {
                std::memcpy(notification.title, "Notification", sizeof("Notification"));
                CopyText(notification.message, sizeof(notification.message), message, message_length);
            }
            state_.notification_count = std::min<uint8_t>(
                static_cast<uint8_t>(state_.notification_count + 1),
                static_cast<uint8_t>(state_.notifications.size()));
            ++notification_events_;
            if (notification_events_ == 10) {
                ESP_LOGI(kTag, "Heap after 10 notifications: free=%u min=%u",
                         static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                         static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)));
            }
            ++state_.revision;
            return true;
        }
        case 0x7c:
            if (length < 7) return false;
            state_.use_24_hour = data[6] == 0;
            ++state_.revision;
            return true;
        case 0x7e: {
            if (length < 8) return false;
            const size_t count = std::min<size_t>((length - 6) / 2, state_.weather.size());
            for (size_t i = 0; i < count; ++i) {
                const uint8_t packed = data[6 + i * 2];
                const int sign = (packed & 1) ? -1 : 1;
                state_.weather[i].icon = packed >> 4;
                state_.weather[i].temperature = static_cast<int8_t>(data[7 + i * 2] * sign);
            }
            state_.weather_count = static_cast<uint8_t>(count);
            ++state_.revision;
            return true;
        }
        case 0x88: {
            if (length < 8) return false;
            const size_t count = std::min<size_t>((length - 6) / 2, state_.weather.size());
            for (size_t i = 0; i < count; ++i) {
                uint8_t high = data[6 + i * 2];
                uint8_t low = data[7 + i * 2];
                state_.weather[i].high = static_cast<int8_t>((high & 0x7f) * ((high & 0x80) ? -1 : 1));
                state_.weather[i].low = static_cast<int8_t>((low & 0x7f) * ((low & 0x80) ? -1 : 1));
            }
            ++state_.revision;
            return true;
        }
        case 0x91:
            if (length < 8 || data[3] != 0xfe) return false;
            state_.phone_battery_valid = true;
            state_.phone_charging = data[6] == 1;
            state_.phone_battery = std::min<uint8_t>(data[7], 100);
            ++state_.revision;
            return true;
        case 0x93: {
            if (length < 14) return false;
            const uint16_t year = static_cast<uint16_t>((data[7] << 8) | data[8]);
            if (year < 2000 || year > 2099 || data[9] < 1 || data[9] > 12 ||
                data[10] < 1 || data[10] > 31 || data[11] > 23 ||
                data[12] > 59 || data[13] > 59) {
                return false;
            }
            state_.year = year;
            state_.month = data[9];
            state_.day = data[10];
            state_.hour = data[11];
            state_.minute = data[12];
            state_.second = data[13];
            state_.time_sync_us = esp_timer_get_time();
            state_.time_valid = true;
            ++state_.revision;
            return true;
        }
        case 0xef: {
            if (length < 6 || data[3] != 0xfe) return false;
            auto& nav = state_.navigation;
            if (data[5] == 0x00) {
                nav = {};
            } else if (data[5] == 0xff) {
                nav = {};
                nav.active = true;
                std::memcpy(nav.title, "Navigation disabled", sizeof("Navigation disabled"));
            } else if (data[5] == 0x80 && length >= 13) {
                ChronosNavigation parsed = {};
                parsed.active = true;
                parsed.is_navigation = data[7] == 1;
                size_t cursor = 12;
                if (!ReadCString(data, length, &cursor, parsed.title, sizeof(parsed.title)) ||
                    !ReadCString(data, length, &cursor, parsed.duration, sizeof(parsed.duration)) ||
                    !ReadCString(data, length, &cursor, parsed.distance, sizeof(parsed.distance)) ||
                    !ReadCString(data, length, &cursor, parsed.eta, sizeof(parsed.eta)) ||
                    !ReadCString(data, length, &cursor, parsed.directions, sizeof(parsed.directions)) ||
                    !ReadCString(data, length, &cursor, parsed.speed, sizeof(parsed.speed))) {
                    return false;
                }
                nav = parsed;
            } else {
                return false;
            }
            ++state_.revision;
            return true;
        }
        default:
            return false;
    }
}
