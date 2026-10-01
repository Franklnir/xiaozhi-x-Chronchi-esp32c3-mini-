#ifndef XIAOZHI_CHRONOS_PROTOCOL_H_
#define XIAOZHI_CHRONOS_PROTOCOL_H_

#include "chronchi_models.h"
#include "chronchi_state.h"

#include <cstddef>
#include <cstdint>

// Handler and parser for the Chronos Android Companion App (by fbiego) binary protocol.
// Chronos operates over Nordic UART Service (NUS) with packets starting with 0xAB or 0xEA.
class ChronosProtocol {
public:
    static constexpr size_t kMaxPacketSize = 512;

    explicit ChronosProtocol(ChronchiState& state);

    // Feed raw incoming BLE chunks from the Chronos RX characteristic.
    // Handles multi-frame packet reassembly if MTU splits packets.
    // Returns true if a complete packet was assembled and handled.
    bool FeedFrame(const uint8_t* data, size_t length);

    void Reset();

    // Chronos Handshake & Status packet builders (to be sent via Chronos TX notify)
    static size_t BuildInfoPacket(uint8_t* buffer, size_t capacity);
    static size_t BuildBatteryPacket(uint8_t* buffer, size_t capacity, uint8_t level, bool charging);
    static size_t BuildEspPacket(uint8_t* buffer, size_t capacity);

    static const char* AppNameForIcon(uint8_t icon);
    static ChronchiManeuver ParseManeuverFromText(const char* text);

private:
    void HandlePacket(const uint8_t* data, size_t length);
    void HandleTimeSync(const uint8_t* data, size_t length);
    void HandlePhoneBattery(const uint8_t* data, size_t length);
    void HandleWeather(const uint8_t* data, size_t length);
    void HandleNotification(const uint8_t* data, size_t length);
    void HandleNavigation(const uint8_t* data, size_t length);
    void HandleMusic(const uint8_t* data, size_t length);

    ChronchiState& state_;
    uint8_t buffer_[kMaxPacketSize] = {};
    size_t expected_length_ = 0;
    size_t current_length_ = 0;

    // Music caching
    char music_title_[64] = {};
    char music_artist_[64] = {};
};

#endif  // XIAOZHI_CHRONOS_PROTOCOL_H_
