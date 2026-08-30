#ifndef XIAOZHI_CHRONCHI_OTA_H_
#define XIAOZHI_CHRONCHI_OTA_H_

#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>

#include <array>
#include <cstddef>
#include <cstdint>

enum class ChronchiOtaStatus : uint8_t {
    Accepted,
    Rejected,
};

struct ChronchiOtaReply {
    ChronchiOtaStatus status = ChronchiOtaStatus::Rejected;
    uint8_t sequence = 0;
    uint8_t packet_type = 0;
    bool completed = false;
    char error[49] = {};
};

class ChronchiOta {
public:
    ChronchiOta();
    ~ChronchiOta();

    static bool IsOtaPacket(uint8_t packet_type);
    ChronchiOtaReply HandleFrame(const uint8_t* data, size_t length);
    void Abort();
    bool Available() const;
    size_t MaximumImageSize() const;
    const char* Strategy() const;
    bool Active() const { return active_; }

private:
    ChronchiOtaReply Begin(uint8_t sequence, const uint8_t* payload, size_t length);
    ChronchiOtaReply WriteChunk(uint8_t sequence, const uint8_t* payload, size_t length);
    ChronchiOtaReply End(uint8_t sequence);
    ChronchiOtaReply EnterRecovery(uint8_t sequence);
    ChronchiOtaReply Reject(uint8_t sequence, uint8_t packet_type, const char* error);
    ChronchiOtaReply Accept(uint8_t sequence, uint8_t packet_type, bool completed = false);
    void ResetState(bool abort_handle);

    const esp_partition_t* partition_ = nullptr;
    esp_ota_handle_t handle_ = 0;
    mbedtls_sha256_context sha_ = {};
    std::array<uint8_t, 32> expected_sha_ = {};
    size_t expected_size_ = 0;
    size_t received_ = 0;
    bool sha_initialized_ = false;
    bool active_ = false;
};

#endif  // XIAOZHI_CHRONCHI_OTA_H_
