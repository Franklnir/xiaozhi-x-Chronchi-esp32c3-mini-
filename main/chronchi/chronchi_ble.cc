#include "chronchi_ble.h"

#include <esp_heap_caps.h>
#include <esp_app_desc.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <host/ble_gatt.h>
#include <host/ble_gap.h>
#include <host/ble_hs.h>
#include <host/ble_hs_mbuf.h>
#include <host/ble_sm.h>
#include <host/ble_store.h>
#include <host/ble_uuid.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <os/os_mbuf.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

#include <array>
#include <cstdio>
#include <cstring>

extern "C" void ble_store_config_init(void);

namespace {
constexpr char kTag[] = "ChronchiBLE";
#if CONFIG_BT_NIMBLE_SECURITY_ENABLE
constexpr bool kSecurityEnabled = true;
#else
constexpr bool kSecurityEnabled = false;
#endif

// ESPBridge Android V1 UUIDs. BLE_UUID128_INIT uses little-endian byte order.
// Service: 7c9e0001-6f2f-4d4d-9f25-0d7fd4f0a001
static const ble_uuid128_t kServiceUuid = BLE_UUID128_INIT(
    0x01, 0xa0, 0xf0, 0xd4, 0x7f, 0x0d, 0x25, 0x9f,
    0x4d, 0x4d, 0x2f, 0x6f, 0x01, 0x00, 0x9e, 0x7c);
// Phone -> ESP32: 7c9e0002-6f2f-4d4d-9f25-0d7fd4f0a001
static const ble_uuid128_t kRxUuid = BLE_UUID128_INIT(
    0x01, 0xa0, 0xf0, 0xd4, 0x7f, 0x0d, 0x25, 0x9f,
    0x4d, 0x4d, 0x2f, 0x6f, 0x02, 0x00, 0x9e, 0x7c);
// ESP32 -> Phone: 7c9e0003-6f2f-4d4d-9f25-0d7fd4f0a001
static const ble_uuid128_t kTxUuid = BLE_UUID128_INIT(
    0x01, 0xa0, 0xf0, 0xd4, 0x7f, 0x0d, 0x25, 0x9f,
    0x4d, 0x4d, 0x2f, 0x6f, 0x03, 0x00, 0x9e, 0x7c);

ChronchiBle* g_instance = nullptr;
uint16_t g_tx_value_handle = 0;

static const ble_gatt_chr_def kCharacteristics[] = {
    {
        .uuid = &kRxUuid.u,
        .access_cb = ChronchiBle::GattAccess,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP
#if CONFIG_BT_NIMBLE_SECURITY_ENABLE
                 | BLE_GATT_CHR_F_WRITE_ENC | BLE_GATT_CHR_F_WRITE_AUTHEN
#endif
        ,
        .min_key_size =
#if CONFIG_BT_NIMBLE_SECURITY_ENABLE
            16,
#else
            0,
#endif
        .val_handle = nullptr,
        .cpfd = nullptr,
    },
    {
        .uuid = &kTxUuid.u,
        .access_cb = ChronchiBle::GattAccess,
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

ChronchiBle::ChronchiBle(ChronchiState& state) : state_(state), protocol_(state) {}

void ChronchiBle::BuildIdentity() {
    uint8_t mac[6] = {};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        ESP_LOGW(kTag, "Cannot read base MAC; using fallback device identity");
        return;
    }
    std::snprintf(device_id_, sizeof(device_id_), "CH-%02X%02X", mac[4], mac[5]);
    std::snprintf(device_name_, sizeof(device_name_), "Chronchi-%02X%02X", mac[4], mac[5]);
}

esp_err_t ChronchiBle::Initialize() {
    if (g_instance != nullptr) return ESP_ERR_INVALID_STATE;
    g_instance = this;
    BuildIdentity();

    esp_err_t error = nimble_port_init();
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "nimble_port_init failed: %s", esp_err_to_name(error));
        g_instance = nullptr;
        return error;
    }

    ble_hs_cfg.reset_cb = OnReset;
    ble_hs_cfg.sync_cb = OnSync;
#if CONFIG_BT_NIMBLE_SECURITY_ENABLE
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_YES_NO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_sc_only = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_store_config_init();
#endif
    ble_svc_gap_init();
    ble_svc_gatt_init();
    int result = ble_gatts_count_cfg(kServices);
    if (result == 0) result = ble_gatts_add_svcs(kServices);
    if (result == 0) result = ble_svc_gap_device_name_set(device_name_);
    if (result != 0) {
        ESP_LOGE(kTag, "GATT initialization failed: rc=%d", result);
        nimble_port_deinit();
        g_instance = nullptr;
        return ESP_FAIL;
    }

    nimble_port_freertos_init(HostTask);
    ESP_LOGI(kTag, "ESPBridge V1 peripheral initialized as %s (%s); MTU=%d max_json=%u secure=%s",
             device_name_, device_id_,
             CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU,
             static_cast<unsigned>(ChronchiProtocol::kMaximumJson),
             kSecurityEnabled ? "yes" : "no");
    return ESP_OK;
}

void ChronchiBle::HostTask(void*) {
    ESP_LOGI(kTag, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ChronchiBle::ClearResponses() {
    std::lock_guard<std::mutex> lock(response_mutex_);
    response_head_ = 0;
    response_tail_ = 0;
    response_count_ = 0;
}

void ChronchiBle::OnReset(int reason) {
    ESP_LOGE(kTag, "NimBLE reset, reason=%d", reason);
    if (g_instance == nullptr) return;
    g_instance->connection_handle_.store(0xffff);
    g_instance->notify_subscribed_.store(false);
    g_instance->link_secure_.store(false);
    g_instance->pending_pairing_handle_.store(0xffff);
    g_instance->ota_.Abort();
    g_instance->ClearResponses();
    g_instance->protocol_.Reset();
    g_instance->state_.SetSubscribed(false);
    g_instance->state_.SetConnection(false);
}

void ChronchiBle::OnSync() {
    if (g_instance == nullptr) return;
    uint8_t address_type = 0;
    const int result = ble_hs_id_infer_auto(0, &address_type);
    if (result != 0) {
        ESP_LOGE(kTag, "Cannot infer BLE address type: rc=%d", result);
        return;
    }
    g_instance->Advertise();
}

void ChronchiBle::Advertise() {
    ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = const_cast<ble_uuid128_t*>(&kServiceUuid);
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    int result = ble_gap_adv_set_fields(&fields);
    if (result != 0) {
        ESP_LOGE(kTag, "Cannot set advertising fields: rc=%d", result);
        return;
    }

    ble_hs_adv_fields response = {};
    response.name = reinterpret_cast<uint8_t*>(device_name_);
    response.name_len = std::strlen(device_name_);
    response.name_is_complete = 1;
    result = ble_gap_adv_rsp_set_fields(&response);
    if (result != 0) {
        ESP_LOGE(kTag, "Cannot set scan response: rc=%d", result);
        return;
    }

    uint8_t address_type = 0;
    result = ble_hs_id_infer_auto(0, &address_type);
    if (result != 0) return;
    ble_gap_adv_params parameters = {};
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
    result = ble_gap_adv_start(address_type, nullptr, BLE_HS_FOREVER,
                               &parameters, GapEvent, this);
    if (result == 0) {
        ESP_LOGI(kTag, "Advertising ESPBridge V1 as '%s'", device_name_);
    } else {
        ESP_LOGE(kTag, "Advertising start failed: rc=%d", result);
    }
}

int ChronchiBle::GapEvent(ble_gap_event* event, void* arg) {
    auto* self = static_cast<ChronchiBle*>(arg);
    if (self == nullptr) self = g_instance;
    if (self == nullptr) return 0;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                self->connection_handle_.store(event->connect.conn_handle);
                self->notify_subscribed_.store(false);
                self->link_secure_.store(!kSecurityEnabled);
                self->pending_pairing_handle_.store(0xffff);
                self->ClearResponses();
                self->protocol_.Reset();
                self->state_.SetConnection(true);
                ESP_LOGI(kTag, "ESPBridge phone connected, handle=%u heap=%u min=%u",
                         static_cast<unsigned>(event->connect.conn_handle),
                         static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                         static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)));
#if CONFIG_BT_NIMBLE_SECURITY_ENABLE
                const int security_result = ble_gap_security_initiate(event->connect.conn_handle);
                if (security_result != 0) {
                    ESP_LOGW(kTag, "Cannot initiate secure pairing: rc=%d", security_result);
                }
#endif
            } else {
                ESP_LOGW(kTag, "Connection failed, status=%d", event->connect.status);
                self->Advertise();
            }
            return 0;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(kTag, "Phone disconnected, reason=%d", event->disconnect.reason);
            self->connection_handle_.store(0xffff);
            self->notify_subscribed_.store(false);
            self->link_secure_.store(false);
            self->pending_pairing_handle_.store(0xffff);
            self->ota_.Abort();
            self->ClearResponses();
            self->protocol_.Reset();
            self->state_.SetSubscribed(false);
            self->state_.SetConnection(false);
            self->Advertise();
            return 0;
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == g_tx_value_handle) {
                const bool subscribed = event->subscribe.cur_notify != 0;
                self->notify_subscribed_.store(subscribed);
                self->state_.SetSubscribed(subscribed);
                if (subscribed && self->link_secure_.load()) {
                    self->QueueResponse(ResponseKind::Ready, 0,
                                        static_cast<uint8_t>(ChronchiPacketType::Ack));
                }
                ESP_LOGI(kTag, "Protocol notifications %s",
                         subscribed ? "subscribed" : "unsubscribed");
            }
            return 0;
        case BLE_GAP_EVENT_ENC_CHANGE:
            if (event->enc_change.status == 0) {
                self->link_secure_.store(true);
                self->pending_pairing_handle_.store(0xffff);
                self->state_.ShowSystem("Pairing aman", "Chronchi terverifikasi");
                if (self->notify_subscribed_.load()) {
                    self->QueueResponse(ResponseKind::Ready, 0,
                                        static_cast<uint8_t>(ChronchiPacketType::Ack));
                }
                ESP_LOGI(kTag, "BLE link encrypted and authenticated");
            } else {
                self->link_secure_.store(false);
                ESP_LOGW(kTag, "BLE encryption failed: status=%d", event->enc_change.status);
            }
            return 0;
        case BLE_GAP_EVENT_PASSKEY_ACTION:
#if CONFIG_BT_NIMBLE_SECURITY_ENABLE
            if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
                self->pending_pairing_handle_.store(event->passkey.conn_handle);
                self->pending_pairing_number_.store(event->passkey.params.numcmp);
                char passkey[24] = {};
                std::snprintf(passkey, sizeof(passkey), "Kode %06lu",
                              static_cast<unsigned long>(event->passkey.params.numcmp));
                self->state_.ShowSystem(passkey, "Tekan tombol untuk OK");
                ESP_LOGI(kTag, "Numeric comparison pending: %06lu",
                         static_cast<unsigned long>(event->passkey.params.numcmp));
                return 0;
            }
#endif
            return BLE_HS_ENOTSUP;
        case BLE_GAP_EVENT_REPEAT_PAIRING: {
#if CONFIG_BT_NIMBLE_SECURITY_ENABLE
            ble_gap_conn_desc description = {};
            if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &description) == 0) {
                ble_store_util_delete_peer(&description.peer_id_addr);
                return BLE_GAP_REPEAT_PAIRING_RETRY;
            }
#endif
            return BLE_GAP_REPEAT_PAIRING_IGNORE;
        }
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

bool ChronchiBle::ConfirmPairing() {
#if CONFIG_BT_NIMBLE_SECURITY_ENABLE
    const uint16_t handle = pending_pairing_handle_.exchange(0xffff);
    if (handle == 0xffff) return false;
    ble_sm_io response = {};
    response.action = BLE_SM_IOACT_NUMCMP;
    response.numcmp_accept = 1;
    const int result = ble_sm_inject_io(handle, &response);
    if (result != 0) {
        ESP_LOGW(kTag, "Pairing confirmation failed: rc=%d", result);
        state_.ShowSystem("Pairing gagal", "Coba hubungkan ulang");
    } else {
        state_.ShowSystem("Kode diterima", "Menyelesaikan pairing");
    }
    return true;
#else
    return false;
#endif
}

void ChronchiBle::QueueResponse(ResponseKind kind, uint8_t sequence, uint8_t packet_type,
                                const char* error) {
    std::lock_guard<std::mutex> lock(response_mutex_);
    if (response_count_ == response_queue_.size()) {
        ESP_LOGW(kTag, "ACK queue full; dropping sequence=%u", static_cast<unsigned>(sequence));
        return;
    }
    PendingResponse& response = response_queue_[response_tail_];
    response = {};
    response.kind = kind;
    response.sequence = sequence;
    response.packet_type = packet_type;
    ChronchiState::CopyText(response.error, sizeof(response.error), error);
    response_tail_ = (response_tail_ + 1) % response_queue_.size();
    ++response_count_;
}

int ChronchiBle::GattAccess(uint16_t, uint16_t, ble_gatt_access_ctxt* context, void*) {
    if (g_instance == nullptr || context == nullptr ||
        context->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    const uint16_t packet_length = OS_MBUF_PKTLEN(context->om);
    if (packet_length == 0 || packet_length > CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    std::array<uint8_t, CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU> packet = {};
    uint16_t copied = 0;
    const int result = ble_hs_mbuf_to_flat(context->om, packet.data(), packet.size(), &copied);
    if (result != 0 || copied != packet_length) return BLE_ATT_ERR_UNLIKELY;

    if (copied >= ChronchiProtocol::kHeaderSize &&
        ChronchiOta::IsOtaPacket(packet[2])) {
        const ChronchiOtaReply reply = g_instance->ota_.HandleFrame(packet.data(), copied);
        if (reply.status == ChronchiOtaStatus::Accepted) {
            if (reply.packet_type != static_cast<uint8_t>(ChronchiPacketType::OtaChunk)) {
                g_instance->QueueResponse(reply.completed ? ResponseKind::OtaComplete
                                                          : ResponseKind::Ack,
                                          reply.sequence, reply.packet_type);
            }
        } else {
            g_instance->QueueResponse(ResponseKind::Nack, reply.sequence, reply.packet_type,
                                      reply.error);
        }
        return 0;
    }

    const ChronchiProtocolResult parsed = g_instance->protocol_.FeedFrame(packet.data(), copied);
    if (parsed == ChronchiProtocolResult::Accepted) {
        const ResponseKind response =
            g_instance->protocol_.LastPacketType() ==
                    static_cast<uint8_t>(ChronchiPacketType::DeviceStatus)
                ? ResponseKind::DeviceInfo
                : ResponseKind::Ack;
        g_instance->QueueResponse(response,
                                  g_instance->protocol_.LastSequence(),
                                  g_instance->protocol_.LastPacketType());
    } else if (parsed == ChronchiProtocolResult::Rejected) {
        ESP_LOGW(kTag, "Rejected ESPBridge frame: %s", g_instance->protocol_.LastError());
        g_instance->QueueResponse(ResponseKind::Nack,
                                  g_instance->protocol_.LastSequence(),
                                  g_instance->protocol_.LastPacketType(),
                                  g_instance->protocol_.LastError());
    }
    return 0;
}

bool ChronchiBle::Notify(const uint8_t* data, size_t length) {
    const uint16_t handle = connection_handle_.load();
    if (!notify_subscribed_.load() || handle == 0xffff || data == nullptr || length == 0 ||
        length > UINT16_MAX) {
        return false;
    }
    os_mbuf* packet = ble_hs_mbuf_from_flat(data, static_cast<uint16_t>(length));
    if (packet == nullptr) return false;
    const int result = ble_gatts_notify_custom(handle, g_tx_value_handle, packet);
    if (result != 0) {
        ESP_LOGW(kTag, "Notify failed: rc=%d", result);
        return false;
    }
    return true;
}

bool ChronchiBle::NotifyResponse(const PendingResponse& response) {
    char json[256] = {};
    ChronchiPacketType response_type = ChronchiPacketType::Ack;
    if (response.kind == ResponseKind::Ready) {
        // Keep every control response within the 20-byte ATT payload available
        // before MTU negotiation. The frame header sequence identifies the
        // original request and the ACK packet type identifies this response, so
        // repeating request metadata would make the handshake depend on a larger MTU.
        std::snprintf(json, sizeof(json), "{\"r\":true}");
    } else if (response.kind == ResponseKind::Ack) {
        std::snprintf(json, sizeof(json), "{\"ok\":true}");
    } else if (response.kind == ResponseKind::OtaComplete) {
        std::snprintf(json, sizeof(json), "{\"ok\":true,\"reboot\":true}");
    } else if (response.kind == ResponseKind::DeviceInfo) {
        response_type = ChronchiPacketType::DeviceStatus;
        const esp_app_desc_t* app = esp_app_get_description();
        const esp_partition_t* running = esp_ota_get_running_partition();
        std::snprintf(
            json, sizeof(json),
            "{\"id\":\"%s\",\"name\":\"%s\",\"fw\":\"%s\",\"board\":\"%s\","
            "\"ota\":%s,\"max\":%u,\"secure\":%s,\"slot\":\"%s\","
            "\"update\":\"%s\",\"role\":\"main\"}",
            device_id_, device_name_, app == nullptr ? "unknown" : app->version, BOARD_NAME,
            ota_.Available() ? "true" : "false",
            static_cast<unsigned>(ota_.MaximumImageSize()),
            kSecurityEnabled ? "true" : "false",
            running == nullptr ? "unknown" : running->label,
            ota_.Strategy());
    } else {
        std::snprintf(json, sizeof(json), "{\"ok\":false,\"error\":\"%.32s\"}",
                      response.error);
    }

    const size_t payload_length = std::strlen(json);
    std::array<uint8_t, ChronchiProtocol::kHeaderSize + sizeof(json)> frame = {};
    frame[0] = ChronchiProtocol::kMagic;
    frame[1] = ChronchiProtocol::kVersion;
    frame[2] = static_cast<uint8_t>(response_type);
    frame[3] = response.sequence;
    frame[4] = 0;
    frame[5] = 1;
    frame[6] = static_cast<uint8_t>((payload_length >> 8) & 0xff);
    frame[7] = static_cast<uint8_t>(payload_length & 0xff);
    std::memcpy(frame.data() + ChronchiProtocol::kHeaderSize, json, payload_length);
    return Notify(frame.data(), ChronchiProtocol::kHeaderSize + payload_length);
}

void ChronchiBle::Poll() {
    const int64_t restart_at = restart_at_us_.load();
    if (restart_at != 0 && esp_timer_get_time() >= restart_at) {
        ESP_LOGI(kTag, "Restarting into verified OTA image");
        esp_restart();
    }
    PendingResponse response = {};
    {
        std::lock_guard<std::mutex> lock(response_mutex_);
        if (response_count_ == 0) return;
        response = response_queue_[response_head_];
    }
    if (!NotifyResponse(response)) return;

    std::lock_guard<std::mutex> lock(response_mutex_);
    if (response_count_ == 0) return;
    response_head_ = (response_head_ + 1) % response_queue_.size();
    --response_count_;
    if (response.kind == ResponseKind::OtaComplete) {
        restart_at_us_.store(esp_timer_get_time() + 800 * 1000);
    }
}
