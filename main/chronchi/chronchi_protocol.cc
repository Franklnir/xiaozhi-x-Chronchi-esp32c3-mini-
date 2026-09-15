#include "chronchi_protocol.h"

#include <cJSON.h>
#include <esp_log.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace {
const cJSON* Item(const cJSON* root, const char* name) {
    return cJSON_GetObjectItemCaseSensitive(root, name);
}

const char* String(const cJSON* root, const char* name, const char* fallback = "") {
    const cJSON* item = Item(root, name);
    return cJSON_IsString(item) && item->valuestring != nullptr ? item->valuestring : fallback;
}

const char* StringEither(const cJSON* root, const char* first, const char* second,
                         const char* fallback = "") {
    const char* value = String(root, first);
    return value[0] != '\0' ? value : String(root, second, fallback);
}

int Integer(const cJSON* root, const char* name, int fallback = 0) {
    const cJSON* item = Item(root, name);
    return cJSON_IsNumber(item) ? item->valueint : fallback;
}

int64_t Integer64(const cJSON* root, const char* name, int64_t fallback = 0) {
    const cJSON* item = Item(root, name);
    return cJSON_IsNumber(item) ? static_cast<int64_t>(item->valuedouble) : fallback;
}

double Number(const cJSON* root, const char* name, double fallback = 0.0) {
    const cJSON* item = Item(root, name);
    return cJSON_IsNumber(item) ? item->valuedouble : fallback;
}

bool Boolean(const cJSON* root, const char* name, bool fallback = false) {
    const cJSON* item = Item(root, name);
    return cJSON_IsBool(item) ? cJSON_IsTrue(item) : fallback;
}

void Uppercase(char* destination, size_t capacity, const char* source) {
    if (capacity == 0) return;
    size_t i = 0;
    for (; source != nullptr && source[i] != '\0' && i + 1 < capacity; ++i) {
        destination[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(source[i])));
    }
    destination[i] = '\0';
}

// Navigation providers do not use one universal spelling for a maneuver.
// Normalize their kebab-case, snake_case, and human-readable forms into
// underscore-separated tokens before classifying the direction.
void NormalizeManeuver(char* destination, size_t capacity, const char* source) {
    if (capacity == 0) return;

    size_t output = 0;
    bool separator_pending = false;
    for (size_t input = 0; source != nullptr && source[input] != '\0'; ++input) {
        const unsigned char character = static_cast<unsigned char>(source[input]);
        if (std::isalnum(character)) {
            if (input > 0 && std::isupper(character) &&
                std::islower(static_cast<unsigned char>(source[input - 1]))) {
                separator_pending = true;
            }
            if (separator_pending && output > 0 && output + 1 < capacity) {
                destination[output++] = '_';
            }
            separator_pending = false;
            if (output + 1 >= capacity) break;
            destination[output++] = static_cast<char>(std::toupper(character));
        } else if (output > 0) {
            separator_pending = true;
        }
    }
    destination[output] = '\0';
}

bool HasManeuverToken(const char* text, const char* token) {
    if (text == nullptr || token == nullptr || token[0] == '\0') return false;
    const size_t token_length = std::strlen(token);
    for (const char* position = std::strstr(text, token); position != nullptr;
         position = std::strstr(position + 1, token)) {
        const bool begins_token = position == text || position[-1] == '_';
        const char after = position[token_length];
        const bool ends_token = after == '\0' || after == '_';
        if (begins_token && ends_token) return true;
    }
    return false;
}

bool EqualsIgnoreCase(const char* left, const char* right) {
    if (left == nullptr || right == nullptr) return false;
    while (*left != '\0' && *right != '\0') {
        if (std::tolower(static_cast<unsigned char>(*left)) !=
            std::tolower(static_cast<unsigned char>(*right))) {
            return false;
        }
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

bool IsKnownPacketType(uint8_t value) {
    return value >= static_cast<uint8_t>(ChronchiPacketType::TimeSync) &&
           value <= static_cast<uint8_t>(ChronchiPacketType::SwitchMode);
}

ChronchiOrderStatus ParseOrderStatus(const char* value) {
    char text[40] = {};
    Uppercase(text, sizeof(text), value);
    if (std::strcmp(text, "CONFIRMED") == 0 ||
        std::strcmp(text, "PESANAN DIKONFIRMASI") == 0) {
        return ChronchiOrderStatus::Confirmed;
    }
    if (std::strcmp(text, "PACKED") == 0 || std::strcmp(text, "SEDANG DIKEMAS") == 0) {
        return ChronchiOrderStatus::Packed;
    }
    if (std::strcmp(text, "SHIPPED") == 0 || std::strcmp(text, "PESANAN DIKIRIM") == 0) {
        return ChronchiOrderStatus::Shipped;
    }
    if (std::strcmp(text, "IN_TRANSIT") == 0 ||
        std::strcmp(text, "DALAM PERJALANAN") == 0) {
        return ChronchiOrderStatus::InTransit;
    }
    if (std::strcmp(text, "OUT_FOR_DELIVERY") == 0 ||
        std::strcmp(text, "SEDANG DIANTAR") == 0) {
        return ChronchiOrderStatus::OutForDelivery;
    }
    if (std::strcmp(text, "DELIVERED") == 0 ||
        std::strcmp(text, "PESANAN DITERIMA") == 0) {
        return ChronchiOrderStatus::Delivered;
    }
    if (std::strcmp(text, "CANCELLED") == 0 || std::strcmp(text, "CANCELED") == 0 ||
        std::strcmp(text, "PESANAN DIBATALKAN") == 0) {
        return ChronchiOrderStatus::Cancelled;
    }
    return ChronchiOrderStatus::Unknown;
}

ChronchiPaymentDirection ParsePaymentDirection(const char* value) {
    if (EqualsIgnoreCase(value, "INCOMING")) return ChronchiPaymentDirection::Incoming;
    if (EqualsIgnoreCase(value, "OUTGOING")) return ChronchiPaymentDirection::Outgoing;
    return ChronchiPaymentDirection::Unknown;
}

ChronchiManeuver ParseManeuver(const char* value) {
    char text[96] = {};
    NormalizeManeuver(text, sizeof(text), value);
    if (text[0] == '\0') return ChronchiManeuver::Unknown;

    // Google Maps commonly uses TURN_LEFT, turn-left, KEEP_LEFT, and
    // ROUNDABOUT_EXIT. Indonesian instructions are accepted as well so the
    // display remains compatible when the bridge sends a localized phrase.
    if (HasManeuverToken(text, "ROUNDABOUT") || HasManeuverToken(text, "BUNDARAN")) {
        return ChronchiManeuver::Roundabout;
    }
    if (HasManeuverToken(text, "ARRIVE") || HasManeuverToken(text, "ARRIVAL") ||
        HasManeuverToken(text, "DESTINATION") || HasManeuverToken(text, "SAMPAI") ||
        HasManeuverToken(text, "TIBA") || HasManeuverToken(text, "TUJUAN")) {
        return ChronchiManeuver::Arrive;
    }

    const bool left = HasManeuverToken(text, "LEFT") || HasManeuverToken(text, "KIRI");
    const bool right = HasManeuverToken(text, "RIGHT") || HasManeuverToken(text, "KANAN");
    const bool slight = HasManeuverToken(text, "SLIGHT") || HasManeuverToken(text, "KEEP") ||
                        HasManeuverToken(text, "BEAR") || HasManeuverToken(text, "FORK") ||
                        HasManeuverToken(text, "MERGE") || HasManeuverToken(text, "SERONG") ||
                        HasManeuverToken(text, "SEDIKIT") || HasManeuverToken(text, "AGAK");
    if (left && slight) return ChronchiManeuver::SlightLeft;
    if (right && slight) return ChronchiManeuver::SlightRight;
    if (left) return ChronchiManeuver::Left;
    if (right) return ChronchiManeuver::Right;
    if (HasManeuverToken(text, "STRAIGHT") || HasManeuverToken(text, "CONTINUE") ||
        HasManeuverToken(text, "LURUS") || HasManeuverToken(text, "LANJUT") ||
        HasManeuverToken(text, "TERUS")) {
        return ChronchiManeuver::Straight;
    }
    return ChronchiManeuver::Unknown;
}

ChronchiManeuver ParseNavigationManeuver(const cJSON* root) {
    // ESPBridge V1 uses "maneuver". The remaining fields support bridges that
    // forward Google Maps' naming without requiring a synchronized app update.
    constexpr const char* kFields[] = {
        "maneuver", "maneuverType", "direction", "instruction",
        "type", "action", "navType", "nav_type",
    };
    for (const char* field : kFields) {
        const ChronchiManeuver maneuver = ParseManeuver(String(root, field));
        if (maneuver != ChronchiManeuver::Unknown) return maneuver;
    }
    // Also try to extract maneuver from the title/directions text
    // (Google Maps sends "Turn right" in the notification title)
    constexpr const char* kTextFields[] = {
        "title", "directions", "primary", "text", "message",
    };
    for (const char* field : kTextFields) {
        const char* text = String(root, field);
        if (text != nullptr && text[0] != '\0') {
            const ChronchiManeuver maneuver = ParseManeuver(text);
            if (maneuver != ChronchiManeuver::Unknown) return maneuver;
        }
    }
    return ChronchiManeuver::Unknown;
}

const char* ManeuverInstruction(ChronchiManeuver maneuver) {
    switch (maneuver) {
        case ChronchiManeuver::Straight: return "lurus";
        case ChronchiManeuver::Left: return "belok kiri";
        case ChronchiManeuver::Right: return "belok kanan";
        case ChronchiManeuver::SlightLeft: return "serong kiri";
        case ChronchiManeuver::SlightRight: return "serong kanan";
        case ChronchiManeuver::Roundabout: return "bundaran";
        case ChronchiManeuver::Arrive: return "sampai";
        case ChronchiManeuver::Unknown: return "lanjutkan";
    }
    return "lanjutkan";
}

void FormatLocalDateTime(int64_t epoch_seconds, int offset_minutes,
                         char* time_text, size_t time_capacity,
                         char* date_text, size_t date_capacity) {
    if (epoch_seconds <= 0) return;
    const std::time_t adjusted = static_cast<std::time_t>(
        epoch_seconds + static_cast<int64_t>(offset_minutes) * 60);
    std::tm local = {};
    if (gmtime_r(&adjusted, &local) == nullptr) return;
    static constexpr const char* kMonths[] = {
        "Januari", "Februari", "Maret", "April", "Mei", "Juni",
        "Juli", "Agustus", "September", "Oktober", "November", "Desember",
    };
    if (time_text != nullptr && time_capacity > 0) {
        std::snprintf(time_text, time_capacity, "%02d:%02d", local.tm_hour, local.tm_min);
    }
    if (date_text != nullptr && date_capacity > 0 && local.tm_mon >= 0 && local.tm_mon < 12) {
        std::snprintf(date_text, date_capacity, "%d %s %d",
                      local.tm_mday, kMonths[local.tm_mon], local.tm_year + 1900);
    }
}
}  // namespace

void ChronchiProtocol::SetError(const char* error) {
    ChronchiState::CopyText(last_error_, sizeof(last_error_), error);
}

void ChronchiProtocol::ResetAssembly() {
    received_ = 0;
    json_[0] = '\0';
    expected_sequence_ = 0;
    expected_packet_type_ = 0;
    expected_fragment_count_ = 0;
    next_fragment_ = 0;
    assembling_ = false;
}

void ChronchiProtocol::Reset() {
    ResetAssembly();
    last_sequence_ = 0;
    last_packet_type_ = 0;
    SetError("");
}

ChronchiProtocolResult ChronchiProtocol::Reject(const char* error) {
    ResetAssembly();
    SetError(error);
    return ChronchiProtocolResult::Rejected;
}

ChronchiProtocolResult ChronchiProtocol::FeedFrame(const uint8_t* data, size_t length) {
    if (data == nullptr || length < kHeaderSize) return Reject("BLE frame too short");
    if (data[0] != kMagic) return Reject("invalid frame magic");
    if (data[1] != kVersion) return Reject("unsupported protocol version");

    const uint8_t packet_type = data[2];
    const uint8_t sequence = data[3];
    const uint8_t fragment_index = data[4];
    const uint8_t fragment_count = data[5];
    const size_t payload_length = (static_cast<size_t>(data[6]) << 8) | data[7];
    last_sequence_ = sequence;
    last_packet_type_ = packet_type;

    if (!IsKnownPacketType(packet_type)) return Reject("unknown packet type");
    if (fragment_count == 0 || fragment_index >= fragment_count) {
        return Reject("invalid fragment metadata");
    }
    if (payload_length != length - kHeaderSize) return Reject("payload length mismatch");

    if (fragment_index == 0) {
        ResetAssembly();
        assembling_ = true;
        expected_sequence_ = sequence;
        expected_packet_type_ = packet_type;
        expected_fragment_count_ = fragment_count;
    } else if (!assembling_ || sequence != expected_sequence_ ||
               packet_type != expected_packet_type_ ||
               fragment_count != expected_fragment_count_) {
        return Reject("fragment stream changed");
    }
    if (fragment_index != next_fragment_) return Reject("fragment out of order");
    if (received_ + payload_length > kMaximumJson) return Reject("JSON payload too large");

    if (payload_length > 0) {
        std::memcpy(json_.data() + received_, data + kHeaderSize, payload_length);
        received_ += payload_length;
    }
    json_[received_] = '\0';
    ++next_fragment_;
    if (next_fragment_ < expected_fragment_count_) {
        SetError("");
        return ChronchiProtocolResult::Incomplete;
    }

    assembling_ = false;
    const ChronchiProtocolResult result = ParseBufferedJson(
        static_cast<ChronchiPacketType>(packet_type));
    received_ = 0;
    json_[0] = '\0';
    return result;
}

ChronchiProtocolResult ChronchiProtocol::ParseBufferedJson(ChronchiPacketType packet_type) {
    const char* parse_end = nullptr;
    cJSON* root = cJSON_ParseWithLengthOpts(json_.data(), received_, &parse_end, false);
    if (root == nullptr || !cJSON_IsObject(root)) {
        if (root != nullptr) cJSON_Delete(root);
        SetError("invalid JSON object");
        return ChronchiProtocolResult::Rejected;
    }
    while (parse_end != nullptr && parse_end < json_.data() + received_ &&
           std::isspace(static_cast<unsigned char>(*parse_end))) {
        ++parse_end;
    }
    if (parse_end != json_.data() + received_) {
        cJSON_Delete(root);
        SetError("trailing JSON data");
        return ChronchiProtocolResult::Rejected;
    }

    bool accepted = true;
    switch (packet_type) {
        case ChronchiPacketType::TimeSync: {
            utc_offset_minutes_ = std::clamp(Integer(root, "utcOffsetMinutes"), -840, 840);
            char time_text[9] = {};
            char date_text[33] = {};
            FormatLocalDateTime(Integer64(root, "epochSeconds"), utc_offset_minutes_,
                                time_text, sizeof(time_text), date_text, sizeof(date_text));
            if (time_text[0] == '\0') {
                accepted = false;
                SetError("epochSeconds required");
            } else {
                state_.UpdateClock(time_text, date_text);
            }
            break;
        }
        case ChronchiPacketType::PhoneStatus:
            state_.UpdatePhoneStatus(
                static_cast<uint8_t>(std::clamp(Integer(root, "batteryLevel"), 0, 100)),
                Boolean(root, "charging"));
            break;
        case ChronchiPacketType::NetworkStatus: {
            const char* transport = String(root, "transport", "offline");
            const char* generation = String(root, "generation");
            const bool wifi = EqualsIgnoreCase(transport, "wifi");
            const char* label = generation[0] != '\0' ? generation
                : (wifi ? "WiFi" : (EqualsIgnoreCase(transport, "cellular") ? "Cell" : "Offline"));
            state_.UpdateNetwork(
                label,
                static_cast<uint8_t>(std::clamp(Integer(root, "signalLevel"), 0, 4)),
                wifi);
            break;
        }
        case ChronchiPacketType::Weather: {
            char weather[17] = {};
            const cJSON* temperature = Item(root, "temperatureC");
            if (cJSON_IsNumber(temperature)) {
                std::snprintf(weather, sizeof(weather), "%.0fC", Number(root, "temperatureC"));
            }
            state_.UpdateWeather(weather, String(root, "location"));
            break;
        }
        case ChronchiPacketType::Notification: {
            char category[24] = {};
            Uppercase(category, sizeof(category), String(root, "category"));
            if (category[0] == '\0') {
                accepted = false;
                SetError("notification category required");
                break;
            }

            // Extract notification data for callback (Xichi mode)
            ChronchiNotificationData notif_data = {};
            ChronchiState::CopyText(notif_data.source_app, sizeof(notif_data.source_app),
                                    StringEither(root, "sourceApp", "app", "ESPBRIDGE"));
            ChronchiState::CopyText(notif_data.category, sizeof(notif_data.category), category);
            ChronchiState::CopyText(notif_data.primary_text, sizeof(notif_data.primary_text),
                                    String(root, "primaryText"));
            ChronchiState::CopyText(notif_data.secondary_text, sizeof(notif_data.secondary_text),
                                    String(root, "secondaryText"));

            // If callback registered (Xichi mode), call it and skip ChronchiState update
            if (on_notification_) {
                on_notification_(notif_data);
                break;
            }

            // Normal Chronchi mode: update ChronchiState for OLED display
            ChronchiScreen screen = {};
            ChronchiState::CopyText(screen.source_app, sizeof(screen.source_app),
                                    StringEither(root, "sourceApp", "app", "ESPBRIDGE"));
            ChronchiState::CopyText(screen.primary, sizeof(screen.primary), String(root, "primaryText"));
            ChronchiState::CopyText(screen.secondary, sizeof(screen.secondary), String(root, "secondaryText"));
            ChronchiState::CopyText(screen.footer, sizeof(screen.footer), String(root, "tertiaryText"));
            ChronchiState::CopyText(screen.time, sizeof(screen.time), String(root, "time"));
            if (screen.time[0] == '\0') {
                char event_time[9] = {};
                const int64_t timestamp_ms = Integer64(root, "timestamp");
                FormatLocalDateTime(timestamp_ms / 1000, utc_offset_minutes_,
                                    event_time, sizeof(event_time), nullptr, 0);
                ChronchiState::CopyText(screen.time, sizeof(screen.time),
                                        event_time[0] ? event_time : "--:--");
            }

            if (std::strcmp(category, "MESSAGE") == 0) {
                screen.type = ChronchiScreenType::Message;
            } else if (std::strcmp(category, "EMAIL") == 0 ||
                       std::strcmp(category, "PROFESSIONAL") == 0) {
                screen.type = ChronchiScreenType::Professional;
            } else if (std::strcmp(category, "PAYMENT") == 0) {
                screen.type = ChronchiScreenType::Payment;
                screen.payment_direction = ParsePaymentDirection(String(root, "paymentDirection"));
            } else if (std::strcmp(category, "ORDER") == 0) {
                screen.type = ChronchiScreenType::Order;
                screen.order_status = ParseOrderStatus(
                    StringEither(root, "orderStatus", "status", screen.primary));
            } else if (std::strcmp(category, "SYSTEM") == 0) {
                screen.type = ChronchiScreenType::System;
            } else {
                accepted = false;
                SetError("unsupported display category");
                break;
            }
            state_.ApplyScreen(screen);
            break;
        }
        case ChronchiPacketType::Navigation: {
            if (!Boolean(root, "active", false)) {
                state_.StopNavigation();
                break;
            }
            ChronchiScreen screen = {};
            screen.type = ChronchiScreenType::Navigation;
            screen.maneuver = ParseNavigationManeuver(root);

            // Debug: log all navigation fields to diagnose maneuver detection
            ESP_LOGI("ChronchiProtocol", "Navigation: maneuver=%d", (int)screen.maneuver);
            {
                cJSON* item = nullptr;
                cJSON_ArrayForEach(item, root) {
                    if (cJSON_IsString(item)) {
                        ESP_LOGI("ChronchiProtocol", "  nav.%s = \"%s\"", item->string, item->valuestring);
                    }
                }
            }
            ChronchiState::CopyText(screen.source_app, sizeof(screen.source_app), "NAVIGATION");
            ChronchiState::CopyText(screen.time, sizeof(screen.time), String(root, "time", "--:--"));
            const char* distance = String(root, "distanceText");
            char instruction[65] = {};
            std::snprintf(instruction, sizeof(instruction), "%s%s%s",
                          distance, distance[0] ? " " : "",
                          ManeuverInstruction(screen.maneuver));
            ChronchiState::CopyText(screen.primary, sizeof(screen.primary), instruction);
            ChronchiState::CopyText(screen.secondary, sizeof(screen.secondary), String(root, "roadName"));
            ChronchiState::CopyText(screen.footer, sizeof(screen.footer),
                                    String(root, "destinationDistanceText"));
            state_.ApplyScreen(screen);
            break;
        }
        case ChronchiPacketType::Location:
            // Coordinates are accepted for protocol completeness. The OLED uses
            // the human-readable location supplied by WEATHER.
            break;
        case ChronchiPacketType::Command: {
            char action[24] = {};
            Uppercase(action, sizeof(action), String(root, "action"));
            if (std::strcmp(action, "DISMISS") == 0) {
                state_.DismissTransient();
            } else if (std::strcmp(action, "NAVIGATION_STOP") == 0) {
                state_.StopNavigation();
            } else {
                accepted = false;
                SetError("unknown command");
            }
            break;
        }
        case ChronchiPacketType::SyncBegin:
        case ChronchiPacketType::SyncEnd:
            break;
        case ChronchiPacketType::DeviceStatus:
            // An empty object is a capability request. The BLE layer sends a
            // DEVICE_STATUS response after protocol validation succeeds.
            break;
        case ChronchiPacketType::Ack:
            accepted = false;
            SetError("phone cannot send this packet type");
            break;
        case ChronchiPacketType::OtaBegin:
        case ChronchiPacketType::OtaChunk:
        case ChronchiPacketType::OtaEnd:
        case ChronchiPacketType::OtaAbort:
        case ChronchiPacketType::OtaEnterRecovery:
            accepted = false;
            SetError("OTA packet reached JSON protocol");
            break;
        case ChronchiPacketType::FirebaseConfig: {
            FirebaseConfigData config = {};
            ChronchiState::CopyText(config.url, sizeof(config.url), String(root, "url"));
            ChronchiState::CopyText(config.uid, sizeof(config.uid), String(root, "uid"));
            ChronchiState::CopyText(config.device_id, sizeof(config.device_id), String(root, "deviceId"));
            ChronchiState::CopyText(config.secret, sizeof(config.secret), String(root, "secret"));

            if (config.url[0] == '\0' || config.secret[0] == '\0') {
                accepted = false;
                SetError("firebase url and secret required");
                break;
            }

            if (on_firebase_config_) {
                on_firebase_config_(config);
                ESP_LOGI("ChronchiProtocol", "Firebase config received: url=%s uid=%s device=%s",
                         config.url, config.uid, config.device_id);
            }
            break;
        }
        case ChronchiPacketType::ClearConfig: {
            if (on_clear_config_) {
                on_clear_config_();
                ESP_LOGI("ChronchiProtocol", "Clear config command received");
            }
            break;
        }
        case ChronchiPacketType::WifiConfig: {
            WifiConfigData wifi_config = {};
            ChronchiState::CopyText(wifi_config.ssid, sizeof(wifi_config.ssid), String(root, "ssid"));
            ChronchiState::CopyText(wifi_config.password, sizeof(wifi_config.password), String(root, "password"));

            if (wifi_config.ssid[0] == '\0') {
                accepted = false;
                SetError("wifi ssid required");
                break;
            }

            if (on_wifi_config_) {
                on_wifi_config_(wifi_config);
                ESP_LOGI("ChronchiProtocol", "WiFi config received: ssid=%s", wifi_config.ssid);
            }
            break;
        }
        case ChronchiPacketType::SwitchMode: {
            const char* target_mode = String(root, "mode");
            if (target_mode[0] == '\0') {
                accepted = false;
                SetError("switch mode target required");
                break;
            }

            if (on_switch_mode_) {
                on_switch_mode_(target_mode);
                ESP_LOGI("ChronchiProtocol", "Switch mode command received: %s", target_mode);
            }
            break;
        }
        case ChronchiPacketType::WifiScan: {
            if (on_wifi_scan_) {
                on_wifi_scan_();
                ESP_LOGI("ChronchiProtocol", "WiFi scan requested");
            }
            break;
        }
        case ChronchiPacketType::FirebaseStatus: {
            if (on_firebase_status_) {
                on_firebase_status_();
                ESP_LOGI("ChronchiProtocol", "Firebase status requested");
            }
            break;
        }
        case ChronchiPacketType::WifiList: {
            // WifiList is a response type, not handled here
            ESP_LOGI("ChronchiProtocol", "WifiList packet received (response type)");
            break;
        }
    }

    cJSON_Delete(root);
    if (accepted) SetError("");
    return accepted ? ChronchiProtocolResult::Accepted : ChronchiProtocolResult::Rejected;
}
