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
    static size_t BuildNotifyBatteryPacket(uint8_t* buffer, size_t capacity, bool state = true);
    static size_t BuildSyncRequestPacket(uint8_t* buffer, size_t capacity);
    static size_t BuildFindPhonePacket(uint8_t* buffer, size_t capacity, bool state);
    static size_t BuildCameraCapturePacket(uint8_t* buffer, size_t capacity);
    static size_t BuildMusicControlPacket(uint8_t* buffer, size_t capacity, uint16_t command);

    static const char* AppNameForIcon(uint8_t icon);
    static ChronchiManeuver ParseManeuverFromText(const char* text1, const char* text2 = nullptr);

    bool IsCameraReady() const { return camera_ready_; }
    bool ConsumeSendEspRequest() {
        const bool req = send_esp_requested_;
        send_esp_requested_ = false;
        return req;
    }

private:
    void HandlePacket(const uint8_t* data, size_t length);
    void HandleTimeSync(const uint8_t* data, size_t length);
    void HandlePhoneBattery(const uint8_t* data, size_t length);
    void HandleWeather(const uint8_t* data, size_t length);
    void HandleNotification(const uint8_t* data, size_t length);
    void HandleNavigation(const uint8_t* data, size_t length);
    void HandleNavigationIcon(const uint8_t* data, size_t length);
    void HandleMusic(const uint8_t* data, size_t length);

    static void Downsample48To32(const uint8_t* src, uint8_t* dst);

    ChronchiState& state_;
    uint8_t buffer_[kMaxPacketSize] = {};
    size_t expected_length_ = 0;
    size_t current_length_ = 0;

    // Navigation icon caching (48x48 1bpp = 288 bytes, 3 chunks of 96)
    uint8_t nav_icon_48x48_[288] = {};
    uint8_t nav_icon_32x32_[128] = {};
    uint32_t nav_icon_crc_ = 0;
    uint8_t nav_icon_pos_mask_ = 0;
    bool nav_icon_valid_ = false;

    // Music caching
    char music_title_[64] = {};
    char music_artist_[64] = {};

    // Chronos companion state flags
    bool camera_ready_ = false;
    bool send_esp_requested_ = false;
};

#endif  // XIAOZHI_CHRONOS_PROTOCOL_H_
