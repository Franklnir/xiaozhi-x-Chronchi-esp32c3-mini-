#ifndef XIAOZHI_CHRONCHI_BLE_H_
#define XIAOZHI_CHRONCHI_BLE_H_

#include "chronchi_protocol.h"
#include "chronchi_ota.h"
#include "chronchi_state.h"

#include <esp_err.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

class ChronchiBle {
public:
    explicit ChronchiBle(ChronchiState& state);
    esp_err_t Initialize();
    void Poll();
    bool ConfirmPairing();

    static int GattAccess(uint16_t connection_handle, uint16_t attribute_handle,
                          struct ble_gatt_access_ctxt* context, void* arg);

private:
    enum class ResponseKind : uint8_t { Ready, Ack, Nack, DeviceInfo, OtaComplete };
    struct PendingResponse {
        ResponseKind kind = ResponseKind::Ready;
        uint8_t sequence = 0;
        uint8_t packet_type = 0;
        char error[49] = {};
    };

    static constexpr size_t kResponseQueueSize = 16;

    static void HostTask(void* arg);
    static void OnSync();
    static void OnReset(int reason);
    static int GapEvent(struct ble_gap_event* event, void* arg);

    void Advertise();
    void BuildIdentity();
    void ClearResponses();
    void QueueResponse(ResponseKind kind, uint8_t sequence, uint8_t packet_type,
                       const char* error = "");
    bool Notify(const uint8_t* data, size_t length);
    bool NotifyResponse(const PendingResponse& response);

    ChronchiState& state_;
    ChronchiProtocol protocol_;
    ChronchiOta ota_;
    std::atomic<uint16_t> connection_handle_{0xffff};
    std::atomic<bool> notify_subscribed_{false};
    std::atomic<bool> link_secure_{false};
    std::atomic<uint16_t> pending_pairing_handle_{0xffff};
    std::atomic<uint32_t> pending_pairing_number_{0};
    std::atomic<int64_t> restart_at_us_{0};
    char device_name_[20] = "Chronchi";
    char device_id_[12] = "CH-0000";
    std::array<PendingResponse, kResponseQueueSize> response_queue_ = {};
    size_t response_head_ = 0;
    size_t response_tail_ = 0;
    size_t response_count_ = 0;
    std::mutex response_mutex_;
};

#endif  // XIAOZHI_CHRONCHI_BLE_H_
