#ifndef XIAOZHI_CHRONCHI_PROTOCOL_H_
#define XIAOZHI_CHRONCHI_PROTOCOL_H_

#include "chronchi_state.h"

#include <array>
#include <cstddef>
#include <cstdint>

enum class ChronchiProtocolResult : uint8_t {
    Incomplete,
    Accepted,
    Rejected,
};

enum class ChronchiPacketType : uint8_t {
    TimeSync = 0x01,
    PhoneStatus = 0x02,
    NetworkStatus = 0x03,
    Weather = 0x04,
    Notification = 0x05,
    Navigation = 0x06,
    Location = 0x07,
    Command = 0x08,
    DeviceStatus = 0x09,
    Ack = 0x0a,
    SyncBegin = 0x0b,
    SyncEnd = 0x0c,
    OtaBegin = 0x0d,
    OtaChunk = 0x0e,
    OtaEnd = 0x0f,
    OtaAbort = 0x10,
    OtaEnterRecovery = 0x11,
};

class ChronchiProtocol {
public:
    static constexpr uint8_t kMagic = 0x45;
    static constexpr uint8_t kVersion = 0x01;
    static constexpr size_t kHeaderSize = 8;
    static constexpr size_t kMaximumJson = 768;

    explicit ChronchiProtocol(ChronchiState& state) : state_(state) {}

    ChronchiProtocolResult FeedFrame(const uint8_t* data, size_t length);
    void Reset();
    const char* LastError() const { return last_error_; }
    uint8_t LastSequence() const { return last_sequence_; }
    uint8_t LastPacketType() const { return last_packet_type_; }

private:
    ChronchiProtocolResult ParseBufferedJson(ChronchiPacketType packet_type);
    ChronchiProtocolResult Reject(const char* error);
    void ResetAssembly();
    void SetError(const char* error);

    ChronchiState& state_;
    std::array<char, kMaximumJson + 1> json_ = {};
    size_t received_ = 0;
    uint8_t expected_sequence_ = 0;
    uint8_t expected_packet_type_ = 0;
    uint8_t expected_fragment_count_ = 0;
    uint8_t next_fragment_ = 0;
    uint8_t last_sequence_ = 0;
    uint8_t last_packet_type_ = 0;
    int utc_offset_minutes_ = 0;
    bool assembling_ = false;
    char last_error_[49] = {};
};

#endif  // XIAOZHI_CHRONCHI_PROTOCOL_H_
