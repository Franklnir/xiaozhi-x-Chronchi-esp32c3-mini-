#include "chronchi_ota.h"

#include "chronchi_protocol.h"
#include "chronchi_state.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <nvs.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace {
constexpr char kTag[] = "ChronchiOTA";

const cJSON* Item(const cJSON* root, const char* name) {
    return cJSON_GetObjectItemCaseSensitive(root, name);
}

const char* String(const cJSON* root, const char* name) {
    const cJSON* item = Item(root, name);
    return cJSON_IsString(item) && item->valuestring != nullptr ? item->valuestring : "";
}

size_t Size(const cJSON* root, const char* name) {
    const cJSON* item = Item(root, name);
    return cJSON_IsNumber(item) && item->valuedouble >= 0
               ? static_cast<size_t>(item->valuedouble)
               : 0;
}

int HexNibble(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    value = static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

bool DecodeSha256(const char* text, std::array<uint8_t, 32>* output) {
    if (text == nullptr || output == nullptr || std::strlen(text) != 64) return false;
    for (size_t i = 0; i < output->size(); ++i) {
        const int high = HexNibble(text[i * 2]);
        const int low = HexNibble(text[i * 2 + 1]);
        if (high < 0 || low < 0) return false;
        (*output)[i] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}
}  // namespace

ChronchiOta::ChronchiOta() {
    mbedtls_sha256_init(&sha_);
}

ChronchiOta::~ChronchiOta() {
    ResetState(true);
    mbedtls_sha256_free(&sha_);
}

bool ChronchiOta::IsOtaPacket(uint8_t packet_type) {
    return packet_type >= static_cast<uint8_t>(ChronchiPacketType::OtaBegin) &&
           packet_type <= static_cast<uint8_t>(ChronchiPacketType::OtaEnterRecovery);
}

bool ChronchiOta::Available() const {
#if CONFIG_CHRONCHI_RECOVERY_OTA
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                    ESP_PARTITION_SUBTYPE_APP_FACTORY,
                                    "rescue") != nullptr &&
           esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                    ESP_PARTITION_SUBTYPE_APP_OTA_0,
                                    "main") != nullptr;
#else
    return esp_ota_get_next_update_partition(nullptr) != nullptr;
#endif
}

size_t ChronchiOta::MaximumImageSize() const {
#if CONFIG_CHRONCHI_RECOVERY_OTA
    const esp_partition_t* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, "main");
#else
    const esp_partition_t* partition = esp_ota_get_next_update_partition(nullptr);
#endif
    return partition == nullptr ? 0 : partition->size;
}

const char* ChronchiOta::Strategy() const {
#if CONFIG_CHRONCHI_RECOVERY_OTA
    return "recovery";
#else
    return Available() ? "ab" : "none";
#endif
}

ChronchiOtaReply ChronchiOta::Reject(uint8_t sequence, uint8_t packet_type,
                                     const char* error) {
    ChronchiOtaReply reply = {};
    reply.status = ChronchiOtaStatus::Rejected;
    reply.sequence = sequence;
    reply.packet_type = packet_type;
    ChronchiState::CopyText(reply.error, sizeof(reply.error), error);
    ESP_LOGW(kTag, "Rejected type=0x%02x sequence=%u: %s", packet_type,
             static_cast<unsigned>(sequence), reply.error);
    return reply;
}

ChronchiOtaReply ChronchiOta::Accept(uint8_t sequence, uint8_t packet_type,
                                     bool completed) {
    ChronchiOtaReply reply = {};
    reply.status = ChronchiOtaStatus::Accepted;
    reply.sequence = sequence;
    reply.packet_type = packet_type;
    reply.completed = completed;
    return reply;
}

void ChronchiOta::ResetState(bool abort_handle) {
    if (active_ && abort_handle && handle_ != 0) {
        esp_ota_abort(handle_);
    }
    mbedtls_sha256_free(&sha_);
    mbedtls_sha256_init(&sha_);
    partition_ = nullptr;
    handle_ = 0;
    expected_sha_.fill(0);
    expected_size_ = 0;
    received_ = 0;
    sha_initialized_ = false;
    active_ = false;
}

void ChronchiOta::Abort() {
    if (active_) ESP_LOGW(kTag, "OTA transfer aborted at %u bytes", static_cast<unsigned>(received_));
    ResetState(true);
}

ChronchiOtaReply ChronchiOta::HandleFrame(const uint8_t* data, size_t length) {
    if (data == nullptr || length < ChronchiProtocol::kHeaderSize) {
        return Reject(0, 0, "OTA frame too short");
    }
    const uint8_t packet_type = data[2];
    const uint8_t sequence = data[3];
    if (data[0] != ChronchiProtocol::kMagic || data[1] != ChronchiProtocol::kVersion) {
        return Reject(sequence, packet_type, "invalid OTA frame header");
    }
    const uint8_t fragment_index = data[4];
    const uint8_t fragment_count = data[5];
    const size_t payload_length = (static_cast<size_t>(data[6]) << 8) | data[7];
    if (fragment_index != 0 || fragment_count != 1) {
        return Reject(sequence, packet_type, "OTA frames must be single fragment");
    }
    if (payload_length != length - ChronchiProtocol::kHeaderSize) {
        return Reject(sequence, packet_type, "OTA payload length mismatch");
    }
    const uint8_t* payload = data + ChronchiProtocol::kHeaderSize;
    switch (static_cast<ChronchiPacketType>(packet_type)) {
        case ChronchiPacketType::OtaBegin: return Begin(sequence, payload, payload_length);
        case ChronchiPacketType::OtaChunk: return WriteChunk(sequence, payload, payload_length);
        case ChronchiPacketType::OtaEnd: return End(sequence);
        case ChronchiPacketType::OtaAbort:
            Abort();
            return Accept(sequence, packet_type);
        case ChronchiPacketType::OtaEnterRecovery: return EnterRecovery(sequence);
        default: return Reject(sequence, packet_type, "unknown OTA packet type");
    }
}

ChronchiOtaReply ChronchiOta::EnterRecovery(uint8_t sequence) {
    const uint8_t packet_type = static_cast<uint8_t>(ChronchiPacketType::OtaEnterRecovery);
#if CONFIG_CHRONCHI_RECOVERY_OTA
    if (active_) return Reject(sequence, packet_type, "OTA already active");
    const esp_partition_t* recovery = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "recovery");
    if (recovery == nullptr) {
        recovery = esp_partition_find_first(
            ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "rescue");
    }
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (recovery == nullptr || running == recovery) {
        return Reject(sequence, packet_type, "recovery partition unavailable");
    }

    nvs_handle_t nvs = 0;
    esp_err_t error = nvs_open("chronchi_ota", NVS_READWRITE, &nvs);
    if (error == ESP_OK) error = nvs_set_u8(nvs, "recovery", 1);
    if (error == ESP_OK) error = nvs_commit(nvs);
    if (nvs != 0) nvs_close(nvs);
    if (error != ESP_OK) return Reject(sequence, packet_type, "cannot persist recovery state");

    error = esp_ota_set_boot_partition(recovery);
    if (error != ESP_OK) return Reject(sequence, packet_type, "cannot select recovery partition");
    ESP_LOGI(kTag, "Recovery updater selected for next boot");
    return Accept(sequence, packet_type, true);
#else
    return Reject(sequence, packet_type, "recovery OTA is not configured");
#endif
}

ChronchiOtaReply ChronchiOta::Begin(uint8_t sequence, const uint8_t* payload, size_t length) {
    const uint8_t packet_type = static_cast<uint8_t>(ChronchiPacketType::OtaBegin);
#if CONFIG_CHRONCHI_RECOVERY_OTA
    return Reject(sequence, packet_type, "enter recovery before firmware transfer");
#endif
    if (active_) return Reject(sequence, packet_type, "OTA already active");
    const char* parse_end = nullptr;
    cJSON* root = cJSON_ParseWithLengthOpts(reinterpret_cast<const char*>(payload), length,
                                            &parse_end, false);
    if (root == nullptr || !cJSON_IsObject(root)) {
        if (root != nullptr) cJSON_Delete(root);
        return Reject(sequence, packet_type, "invalid OTA metadata");
    }
    const char* board = String(root, "board");
    const size_t image_size = Size(root, "size");
    const char* sha256 = String(root, "sha256");
    const bool board_ok = std::strcmp(board, BOARD_NAME) == 0;
    std::array<uint8_t, 32> digest = {};
    const bool hash_ok = DecodeSha256(sha256, &digest);
    cJSON_Delete(root);

    partition_ = esp_ota_get_next_update_partition(nullptr);
    if (partition_ == nullptr) return Reject(sequence, packet_type, "A/B OTA unavailable");
    if (!board_ok) return Reject(sequence, packet_type, "firmware board mismatch");
    if (!hash_ok) return Reject(sequence, packet_type, "invalid SHA-256");
    if (image_size == 0 || image_size > partition_->size) {
        return Reject(sequence, packet_type, "firmware does not fit OTA slot");
    }

    esp_err_t error = esp_ota_begin(partition_, image_size, &handle_);
    if (error != ESP_OK) {
        partition_ = nullptr;
        handle_ = 0;
        return Reject(sequence, packet_type, "cannot open OTA slot");
    }
    if (mbedtls_sha256_starts(&sha_, false) != 0) {
        esp_ota_abort(handle_);
        handle_ = 0;
        partition_ = nullptr;
        return Reject(sequence, packet_type, "SHA-256 init failed");
    }
    expected_sha_ = digest;
    expected_size_ = image_size;
    received_ = 0;
    sha_initialized_ = true;
    active_ = true;
    ESP_LOGI(kTag, "OTA started: partition=%s size=%u board=%s", partition_->label,
             static_cast<unsigned>(expected_size_), BOARD_NAME);
    return Accept(sequence, packet_type);
}

ChronchiOtaReply ChronchiOta::WriteChunk(uint8_t sequence, const uint8_t* payload,
                                         size_t length) {
    const uint8_t packet_type = static_cast<uint8_t>(ChronchiPacketType::OtaChunk);
    if (!active_ || handle_ == 0) return Reject(sequence, packet_type, "OTA not active");
    if (length <= 4) return Reject(sequence, packet_type, "OTA chunk is empty");
    const size_t offset = (static_cast<size_t>(payload[0]) << 24) |
                          (static_cast<size_t>(payload[1]) << 16) |
                          (static_cast<size_t>(payload[2]) << 8) |
                          static_cast<size_t>(payload[3]);
    const size_t chunk_size = length - 4;
    if (offset != received_) return Reject(sequence, packet_type, "OTA offset mismatch");
    if (received_ + chunk_size > expected_size_) {
        Abort();
        return Reject(sequence, packet_type, "OTA exceeds declared size");
    }
    const uint8_t* chunk = payload + 4;
    if (esp_ota_write(handle_, chunk, chunk_size) != ESP_OK ||
        mbedtls_sha256_update(&sha_, chunk, chunk_size) != 0) {
        Abort();
        return Reject(sequence, packet_type, "OTA flash write failed");
    }
    received_ += chunk_size;
    return Accept(sequence, packet_type);
}

ChronchiOtaReply ChronchiOta::End(uint8_t sequence) {
    const uint8_t packet_type = static_cast<uint8_t>(ChronchiPacketType::OtaEnd);
    if (!active_ || handle_ == 0) return Reject(sequence, packet_type, "OTA not active");
    if (received_ != expected_size_) return Reject(sequence, packet_type, "OTA image incomplete");

    std::array<uint8_t, 32> actual_sha = {};
    if (mbedtls_sha256_finish(&sha_, actual_sha.data()) != 0) {
        Abort();
        return Reject(sequence, packet_type, "SHA-256 finish failed");
    }
    sha_initialized_ = false;
    if (actual_sha != expected_sha_) {
        Abort();
        return Reject(sequence, packet_type, "firmware SHA-256 mismatch");
    }

    const esp_ota_handle_t completed_handle = handle_;
    const esp_partition_t* completed_partition = partition_;
    handle_ = 0;
    active_ = false;
    esp_err_t error = esp_ota_end(completed_handle);
    if (error == ESP_OK) error = esp_ota_set_boot_partition(completed_partition);
    ResetState(false);
    if (error != ESP_OK) return Reject(sequence, packet_type, "firmware image validation failed");
    ESP_LOGI(kTag, "OTA verified; next boot partition=%s", completed_partition->label);
    return Accept(sequence, packet_type, true);
}
