#include "chronos_ble.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <host/ble_gatt.h>
#include <host/ble_gap.h>
#include <host/ble_hs.h>
#include <host/ble_hs_mbuf.h>
#include <host/ble_uuid.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <os/os_mbuf.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

#include <cstring>

namespace {
constexpr char kTag[] = "ChronosBLE";
constexpr char kDeviceName[] = "Xiaozhi-Mochi";

// 6e400001-b5a3-f393-e0a9-e50e24dcca9e (Nordic UART service used by Chronos).
static const ble_uuid128_t kServiceUuid = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);
static const ble_uuid128_t kRxUuid = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);
static const ble_uuid128_t kTxUuid = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

ChronosBle* g_instance = nullptr;
uint16_t g_tx_value_handle = 0;

static const ble_gatt_chr_def kCharacteristics[] = {
    {
        .uuid = &kRxUuid.u,
        .access_cb = ChronosBle::GattAccess,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
        .min_key_size = 0,
        .val_handle = nullptr,
        .cpfd = nullptr,
    },
    {
        .uuid = &kTxUuid.u,
        // NimBLE validates that every characteristic has an access callback,
        // including notify-only values. The callback is not invoked for
        // ble_gatts_notify_custom(), but it must still be present so the GATT
        // table can be registered.
        .access_cb = ChronosBle::GattAccess,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_NOTIFY,
        .min_key_size = 0,
        .val_handle = &g_tx_value_handle,
        .cpfd = nullptr,
    },
    {},
};

static const ble_gatt_svc_def kServices[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &kServiceUuid.u,
        .includes = nullptr,
        .characteristics = kCharacteristics,
    },
    {},
};
}  // namespace

ChronosBle::ChronosBle(ChronosState& state) : state_(state), protocol_(state) {}

esp_err_t ChronosBle::Initialize() {
    if (g_instance != nullptr) return ESP_ERR_INVALID_STATE;
    g_instance = this;

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "nimble_port_init failed: %s", esp_err_to_name(err));
        g_instance = nullptr;
        return err;
    }

    ble_hs_cfg.reset_cb = OnReset;
    ble_hs_cfg.sync_cb = OnSync;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    int rc = ble_gatts_count_cfg(kServices);
    if (rc == 0) rc = ble_gatts_add_svcs(kServices);
    if (rc == 0) rc = ble_svc_gap_device_name_set(kDeviceName);
    if (rc != 0) {
        ESP_LOGE(kTag, "GATT initialization failed: rc=%d", rc);
        nimble_port_deinit();
        g_instance = nullptr;
        return ESP_FAIL;
    }

    initialized_.store(true);
    nimble_port_freertos_init(HostTask);
    ESP_LOGI(kTag, "Native ESP-NimBLE peripheral initialized; MTU=%d",
             CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU);
    return ESP_OK;
}

void ChronosBle::HostTask(void*) {
    ESP_LOGI(kTag, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ChronosBle::OnReset(int reason) {
    ESP_LOGE(kTag, "NimBLE reset, reason=%d", reason);
    if (g_instance != nullptr) {
        g_instance->connection_handle_.store(0xffff);
        g_instance->notify_subscribed_.store(false);
        g_instance->handshake_pending_.store(false);
        g_instance->state_.SetSubscribed(false);
        g_instance->state_.SetConnection(false);
        g_instance->protocol_.Reset();
    }
}

void ChronosBle::OnSync() {
    if (g_instance == nullptr) return;
    uint8_t address_type = 0;
    int rc = ble_hs_id_infer_auto(0, &address_type);
    if (rc != 0) {
        ESP_LOGE(kTag, "Cannot infer BLE address type: rc=%d", rc);
        return;
    }
    g_instance->Advertise();
}

void ChronosBle::Advertise() {
    ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = const_cast<ble_uuid128_t*>(&kServiceUuid);
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(kTag, "Cannot set advertising fields: rc=%d", rc);
        return;
    }

    ble_hs_adv_fields response = {};
    response.name = reinterpret_cast<uint8_t*>(const_cast<char*>(kDeviceName));
    response.name_len = std::strlen(kDeviceName);
    response.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&response);
    if (rc != 0) {
        ESP_LOGE(kTag, "Cannot set scan response: rc=%d", rc);
        return;
    }

    uint8_t address_type = 0;
    rc = ble_hs_id_infer_auto(0, &address_type);
    if (rc != 0) return;
    ble_gap_adv_params parameters = {};
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(address_type, nullptr, BLE_HS_FOREVER,
                           &parameters, GapEvent, this);
    if (rc == 0) {
        ESP_LOGI(kTag, "Advertising as '%s'", kDeviceName);
    } else {
        ESP_LOGE(kTag, "Advertising start failed: rc=%d", rc);
    }
}

int ChronosBle::GapEvent(ble_gap_event* event, void* arg) {
    auto* self = static_cast<ChronosBle*>(arg);
    if (self == nullptr) self = g_instance;
    if (self == nullptr) return 0;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                self->connection_handle_ = event->connect.conn_handle;
                self->notify_subscribed_ = false;
                self->state_.SetConnection(true);
                ESP_LOGI(kTag, "Phone connected, handle=%u, heap free=%u min=%u",
                         static_cast<unsigned>(self->connection_handle_.load()),
                         static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                         static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)));
            } else {
                ESP_LOGW(kTag, "Connection failed, status=%d", event->connect.status);
                self->Advertise();
            }
            return 0;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(kTag, "Phone disconnected, reason=%d", event->disconnect.reason);
            self->connection_handle_ = 0xffff;
            self->notify_subscribed_ = false;
            self->state_.SetSubscribed(false);
            self->state_.SetConnection(false);
            self->protocol_.Reset();
            self->Advertise();
            return 0;
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == g_tx_value_handle) {
                self->notify_subscribed_ = event->subscribe.cur_notify != 0;
                self->state_.SetSubscribed(self->notify_subscribed_);
                if (self->notify_subscribed_) self->handshake_pending_.store(true);
                ESP_LOGI(kTag, "TX notifications %s",
                         self->notify_subscribed_ ? "subscribed" : "unsubscribed");
            }
            return 0;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            self->Advertise();
            return 0;
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(kTag, "MTU updated to %u", event->mtu.value);
            return 0;
        default:
            return 0;
    }
}

int ChronosBle::GattAccess(uint16_t, uint16_t, ble_gatt_access_ctxt* context, void*) {
    if (g_instance == nullptr || context == nullptr ||
        context->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    const uint16_t packet_length = OS_MBUF_PKTLEN(context->om);
    if (packet_length == 0 || packet_length > CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    uint8_t packet[CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU] = {};
    uint16_t copied = 0;
    int rc = ble_hs_mbuf_to_flat(context->om, packet, sizeof(packet), &copied);
    if (rc != 0 || copied != packet_length) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    if (!g_instance->protocol_.FeedChunk(packet, copied)) {
        // The app may send optional Chronos opcodes that this compact port does
        // not consume. Keep that diagnostic at debug level to avoid log spam.
        ESP_LOGD("ChronosProtocol", "Rejected or ignored Chronos packet (%u bytes)", copied);
    }
    return 0;
}

bool ChronosBle::Notify(const uint8_t* data, size_t length) {
    const uint16_t connection_handle = connection_handle_.load();
    if (!notify_subscribed_.load() || connection_handle == 0xffff || length == 0) return false;
    os_mbuf* packet = ble_hs_mbuf_from_flat(data, static_cast<uint16_t>(length));
    if (packet == nullptr) return false;
    const int rc = ble_gatts_notify_custom(connection_handle, g_tx_value_handle, packet);
    if (rc != 0) {
        ESP_LOGW(kTag, "Notify failed: rc=%d", rc);
        return false;
    }
    return true;
}

void ChronosBle::Poll() {
    if (!notify_subscribed_.load()) {
        handshake_pending_.store(false);
        handshake_step_ = 0;
        return;
    }

    const int64_t now_us = esp_timer_get_time();
    if (handshake_pending_.exchange(false)) {
        // ChronosESP32 1.9.1 waits before its device-info exchange. Keeping that
        // delay also gives the phone time to finish enabling notifications.
        handshake_step_ = 1;
        next_handshake_us_ = now_us + 3000000;
    }
    if (handshake_step_ == 0 || now_us < next_handshake_us_) return;

    static constexpr uint8_t kDeviceInfo[] = {
        // ChronosESP32 1.9.1; screen=0 because its enum has no 128x64 entry.
        0xab, 0x00, 0x11, 0xff, 0x92, 0xc0, 0x01, 0x5b, 0x00, 0xfb,
        0x1e, 0x40, 0xc0, 0x0e, 0x32, 0x28, 0x00, 0xe2, 0x00, 0x80};
    static constexpr uint8_t kRequestTime[] = {0xab, 0x00, 0x03, 0xfe, 0x23, 0x80};
    static constexpr uint8_t kRequestPhoneBattery[] = {0xab, 0x00, 0x04, 0xfe, 0x91, 0x80, 0x01};

    bool sent = false;
    switch (handshake_step_) {
        case 1:
            sent = Notify(kDeviceInfo, sizeof(kDeviceInfo));
            break;
        case 2:
            sent = Notify(kRequestTime, sizeof(kRequestTime));
            break;
        case 3:
            sent = Notify(kRequestPhoneBattery, sizeof(kRequestPhoneBattery));
            break;
        default:
            handshake_step_ = 0;
            return;
    }
    if (!sent) {
        next_handshake_us_ = now_us + 250000;
        return;
    }

    if (++handshake_step_ > 3) {
        handshake_step_ = 0;
        ESP_LOGI(kTag, "Chronos handshake/time/battery requests sent");
    } else {
        next_handshake_us_ = now_us + 250000;
    }
}
