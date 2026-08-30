#ifndef XIAOZHI_CHRONOS_BLE_H_
#define XIAOZHI_CHRONOS_BLE_H_

#include "chronos_protocol.h"
#include "chronos_state.h"

#include <esp_err.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

class ChronosBle {
public:
    explicit ChronosBle(ChronosState& state);
    esp_err_t Initialize();
    void Poll();

    // C callback referenced by the static native NimBLE GATT table.
    static int GattAccess(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt* context, void* arg);

private:
    static void HostTask(void* arg);
    static void OnSync();
    static void OnReset(int reason);
    static int GapEvent(struct ble_gap_event* event, void* arg);

    void Advertise();
    bool Notify(const uint8_t* data, size_t length);

    ChronosState& state_;
    ChronosProtocol protocol_;
    std::atomic<bool> handshake_pending_{false};
    uint8_t handshake_step_{0};
    int64_t next_handshake_us_{0};
    std::atomic<bool> initialized_{false};
    std::atomic<uint16_t> connection_handle_{0xffff};
    std::atomic<bool> notify_subscribed_{false};
};

#endif  // XIAOZHI_CHRONOS_BLE_H_
