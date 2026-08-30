#include "chronchi_protocol.h"

#include <cJSON.h>

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
           value <= static_cast<uint8_t>(ChronchiPacketType::OtaEnterRecovery);
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
    char text[24] = {};
    Uppercase(text, sizeof(text), value);
    if (std::strcmp(text, "STRAIGHT") == 0) return ChronchiManeuver::Straight;
    if (std::strcmp(text, "LEFT") == 0) return ChronchiManeuver::Left;
    if (std::strcmp(text, "RIGHT") == 0) return ChronchiManeuver::Right;
    if (std::strcmp(text, "SLIGHT_LEFT") == 0) return ChronchiManeuver::SlightLeft;
    if (std::strcmp(text, "SLIGHT_RIGHT") == 0) return ChronchiManeuver::SlightRight;
    if (std::strcmp(text, "ROUNDABOUT") == 0) return ChronchiManeuver::Roundabout;
    if (std::strcmp(text, "ARRIVE") == 0) return ChronchiManeuver::Arrive;
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
            screen.maneuver = ParseManeuver(String(root, "maneuver"));
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
    }

    cJSON_Delete(root);
    if (accepted) SetError("");
    return accepted ? ChronchiProtocolResult::Accepted : ChronchiProtocolResult::Rejected;
}
