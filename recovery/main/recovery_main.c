#include "ota_public_key.h"

#include <driver/gpio.h>
#include <esp_app_desc.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <host/ble_gatt.h>
#include <host/ble_gap.h>
#include <host/ble_hs.h>
#include <host/ble_hs_mbuf.h>
#include <host/ble_sm.h>
#include <host/ble_store.h>
#include <host/ble_uuid.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <os/os_mbuf.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern void ble_store_config_init(void);

#define TAG "ChronchiRecovery"
#define PROTOCOL_MAGIC 0x45
#define PROTOCOL_VERSION 0x01
#define PROTOCOL_HEADER_SIZE 8
#define PACKET_DEVICE_STATUS 0x09
#define PACKET_ACK 0x0a
#define PACKET_OTA_BEGIN 0x0d
#define PACKET_OTA_CHUNK 0x0e
#define PACKET_OTA_END 0x0f
#define PACKET_OTA_ABORT 0x10
#define MAIN_PARTITION_LABEL "main"
#define RECOVERY_NAMESPACE "chronchi_ota"
#define RECOVERY_KEY "recovery"
#define MIN_VERSION_KEY "min_version"
#define RECOVERY_BUTTON GPIO_NUM_3

static const ble_uuid128_t k_service_uuid = BLE_UUID128_INIT(
    0x01, 0xa0, 0xf0, 0xd4, 0x7f, 0x0d, 0x25, 0x9f,
    0x4d, 0x4d, 0x2f, 0x6f, 0x01, 0x00, 0x9e, 0x7c);
static const ble_uuid128_t k_rx_uuid = BLE_UUID128_INIT(
    0x01, 0xa0, 0xf0, 0xd4, 0x7f, 0x0d, 0x25, 0x9f,
    0x4d, 0x4d, 0x2f, 0x6f, 0x02, 0x00, 0x9e, 0x7c);
static const ble_uuid128_t k_tx_uuid = BLE_UUID128_INIT(
    0x01, 0xa0, 0xf0, 0xd4, 0x7f, 0x0d, 0x25, 0x9f,
    0x4d, 0x4d, 0x2f, 0x6f, 0x03, 0x00, 0x9e, 0x7c);

typedef struct {
    const esp_partition_t *partition;
    esp_ota_handle_t handle;
    mbedtls_sha256_context sha;
    uint8_t expected_sha[32];
    char version[32];
    size_t expected_size;
    size_t received;
    bool active;
} ota_session_t;

static ota_session_t s_ota;
static uint16_t s_connection_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_tx_value_handle;
static bool s_notify_subscribed;
static bool s_link_encrypted;
static char s_device_id[12] = "CH-0000";
static char s_device_name[24] = "Chronchi-R0000";

static int gap_event(struct ble_gap_event *event, void *argument);

static void ota_abort(void) {
    if (s_ota.active && s_ota.handle != 0) {
        esp_ota_abort(s_ota.handle);
    }
    mbedtls_sha256_free(&s_ota.sha);
    memset(&s_ota, 0, sizeof(s_ota));
    mbedtls_sha256_init(&s_ota.sha);
}

static bool set_recovery_requested(bool requested) {
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(RECOVERY_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        if (requested) {
            err = nvs_set_u8(handle, RECOVERY_KEY, 1);
        } else {
            err = nvs_erase_key(handle, RECOVERY_KEY);
            if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
        }
    }
    if (err == ESP_OK) err = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    return err == ESP_OK;
}

static bool recovery_requested(void) {
    nvs_handle_t handle = 0;
    uint8_t requested = 0;
    esp_err_t err = nvs_open(RECOVERY_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) err = nvs_get_u8(handle, RECOVERY_KEY, &requested);
    if (handle != 0) nvs_close(handle);
    return err == ESP_OK && requested == 1;
}

static bool set_minimum_version(const char *version) {
    if (version == NULL || version[0] == '\0' || strlen(version) >= sizeof(s_ota.version)) {
        return false;
    }
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(RECOVERY_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) err = nvs_set_str(handle, MIN_VERSION_KEY, version);
    if (err == ESP_OK) err = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    return err == ESP_OK;
}

static bool get_minimum_version(char *version, size_t capacity) {
    if (version == NULL || capacity == 0) return false;
    version[0] = '\0';
    nvs_handle_t handle = 0;
    size_t required = capacity;
    esp_err_t err = nvs_open(RECOVERY_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) err = nvs_get_str(handle, MIN_VERSION_KEY, version, &required);
    if (handle != 0) nvs_close(handle);
    if (err != ESP_OK) version[0] = '\0';
    return err == ESP_OK;
}

static bool parse_release_version(const char *version, uint32_t parts[3]) {
    if (version == NULL || parts == NULL) return false;
    const char *cursor = version;
    for (size_t index = 0; index < 3; ++index) {
        if (!isdigit((unsigned char)*cursor)) return false;
        uint32_t value = 0;
        do {
            const uint32_t digit = (uint32_t)(*cursor - '0');
            if (value > 999999U || value * 10U + digit > 999999U) return false;
            value = value * 10U + digit;
            ++cursor;
        } while (isdigit((unsigned char)*cursor));
        parts[index] = value;
        if (index < 2) {
            if (*cursor++ != '.') return false;
        } else if (*cursor != '\0') {
            return false;
        }
    }
    return true;
}

static int compare_release_versions(const uint32_t left[3], const uint32_t right[3]) {
    for (size_t index = 0; index < 3; ++index) {
        if (left[index] < right[index]) return -1;
        if (left[index] > right[index]) return 1;
    }
    return 0;
}

static const char *enforce_anti_rollback(const esp_partition_t *partition,
                                         const char *incoming_version) {
    uint32_t incoming[3];
    if (!parse_release_version(incoming_version, incoming)) {
        return "version must be numeric x.y.z";
    }

    char minimum_version[32] = {0};
    uint32_t minimum[3];
    bool has_minimum = get_minimum_version(minimum_version, sizeof(minimum_version)) &&
                       parse_release_version(minimum_version, minimum);
    if (has_minimum && compare_release_versions(incoming, minimum) < 0) {
        return "firmware downgrade rejected";
    }

    esp_app_desc_t installed = {0};
    uint32_t installed_parts[3];
    if (esp_ota_get_partition_description(partition, &installed) == ESP_OK &&
        parse_release_version(installed.version, installed_parts)) {
        if (compare_release_versions(incoming, installed_parts) < 0) {
            return "firmware downgrade rejected";
        }
        if (!has_minimum || compare_release_versions(installed_parts, minimum) > 0) {
            if (!set_minimum_version(installed.version)) {
                return "cannot persist version floor";
            }
        }
    }
    return NULL;
}

static const esp_partition_t *main_partition(void) {
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                    ESP_PARTITION_SUBTYPE_APP_OTA_0,
                                    MAIN_PARTITION_LABEL);
}

static void restart_task(void *argument) {
    const uint32_t delay_ms = (uint32_t)(uintptr_t)argument;
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    esp_restart();
}

static void schedule_restart(uint32_t delay_ms) {
    xTaskCreate(restart_task, "recovery_restart", 2048,
                (void *)(uintptr_t)delay_ms, 5, NULL);
}

static bool notify_json(uint8_t type, uint8_t sequence, const char *json) {
    if (!s_notify_subscribed || s_connection_handle == BLE_HS_CONN_HANDLE_NONE || json == NULL) {
        return false;
    }
    const size_t payload_size = strlen(json);
    if (payload_size > 230) return false;
    uint8_t frame[PROTOCOL_HEADER_SIZE + 230] = {0};
    frame[0] = PROTOCOL_MAGIC;
    frame[1] = PROTOCOL_VERSION;
    frame[2] = type;
    frame[3] = sequence;
    frame[4] = 0;
    frame[5] = 1;
    frame[6] = (uint8_t)(payload_size >> 8);
    frame[7] = (uint8_t)payload_size;
    memcpy(frame + PROTOCOL_HEADER_SIZE, json, payload_size);
    struct os_mbuf *packet = ble_hs_mbuf_from_flat(frame, PROTOCOL_HEADER_SIZE + payload_size);
    return packet != NULL &&
           ble_gatts_notify_custom(s_connection_handle, s_tx_value_handle, packet) == 0;
}

static void notify_ack(uint8_t sequence, bool ok, const char *error, bool reboot) {
    char json[128];
    if (ok && reboot) {
        snprintf(json, sizeof(json), "{\"ok\":true,\"reboot\":true}");
    } else if (ok) {
        snprintf(json, sizeof(json), "{\"ok\":true}");
    } else {
        snprintf(json, sizeof(json), "{\"ok\":false,\"error\":\"%.64s\"}",
                 error == NULL ? "recovery error" : error);
    }
    notify_json(PACKET_ACK, sequence, json);
}

static bool verify_release_signature(const char *board, const char *version,
                                     size_t image_size, const uint8_t sha[32],
                                     const uint8_t *signature, size_t signature_size) {
    char sha_hex[65] = {0};
    for (size_t index = 0; index < 32; ++index) {
        snprintf(sha_hex + index * 2, 3, "%02x", sha[index]);
    }
    char canonical[192];
    const int length = snprintf(canonical, sizeof(canonical),
                                "ESPBridge-OTA-v1\n%s\n%s\n%u\n%s",
                                board, version, (unsigned)image_size, sha_hex);
    if (length <= 0 || (size_t)length >= sizeof(canonical)) return false;

    uint8_t digest[32];
    if (mbedtls_sha256((const uint8_t *)canonical, (size_t)length, digest, false) != 0) {
        return false;
    }
    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    int result = mbedtls_pk_parse_public_key(&key, kEspBridgeOtaPublicKeyDer,
                                             kEspBridgeOtaPublicKeyDerSize);
    if (result == 0) {
        result = mbedtls_pk_verify(&key, MBEDTLS_MD_SHA256, digest, sizeof(digest),
                                   signature, signature_size);
    }
    mbedtls_pk_free(&key);
    return result == 0;
}

// OTA_BEGIN binary payload:
// uint32 BE size, 32-byte SHA256, uint8 board_len + board,
// uint8 version_len + version, uint8 DER_signature_len + signature.
static const char *ota_begin(const uint8_t *payload, size_t length) {
    ota_abort();
    if (length < 4 + 32 + 1 + 1 + 1) return "metadata too short";
    size_t cursor = 0;
    const size_t image_size = ((size_t)payload[0] << 24) | ((size_t)payload[1] << 16) |
                              ((size_t)payload[2] << 8) | payload[3];
    cursor += 4;
    uint8_t expected_sha[32];
    memcpy(expected_sha, payload + cursor, sizeof(expected_sha));
    cursor += sizeof(expected_sha);

    const size_t board_size = payload[cursor++];
    if (board_size == 0 || board_size >= 64 || cursor + board_size + 1 > length) {
        return "invalid board metadata";
    }
    char board[64] = {0};
    memcpy(board, payload + cursor, board_size);
    cursor += board_size;

    const size_t version_size = payload[cursor++];
    if (version_size == 0 || version_size >= 32 || cursor + version_size + 1 > length) {
        return "invalid version metadata";
    }
    char version[32] = {0};
    memcpy(version, payload + cursor, version_size);
    cursor += version_size;

    const size_t signature_size = payload[cursor++];
    if (signature_size < 64 || signature_size > 80 || cursor + signature_size != length) {
        return "invalid release signature";
    }
    const esp_partition_t *partition = main_partition();
    if (partition == NULL) return "main partition unavailable";
    if (strcmp(board, RECOVERY_BOARD_NAME) != 0) return "firmware board mismatch";
    if (image_size == 0 || image_size > partition->size) return "firmware does not fit";
    if (!verify_release_signature(board, version, image_size, expected_sha,
                                  payload + cursor, signature_size)) {
        return "release signature rejected";
    }
    const char *version_error = enforce_anti_rollback(partition, version);
    if (version_error != NULL) return version_error;

    esp_ota_handle_t handle = 0;
    if (esp_ota_begin(partition, image_size, &handle) != ESP_OK) return "cannot erase main slot";
    if (mbedtls_sha256_starts(&s_ota.sha, false) != 0) {
        esp_ota_abort(handle);
        return "SHA-256 init failed";
    }
    s_ota.partition = partition;
    s_ota.handle = handle;
    memcpy(s_ota.expected_sha, expected_sha, sizeof(expected_sha));
    memcpy(s_ota.version, version, version_size + 1);
    s_ota.expected_size = image_size;
    s_ota.received = 0;
    s_ota.active = true;
    ESP_LOGI(TAG, "Signed update accepted: version=%s size=%u", version, (unsigned)image_size);
    return NULL;
}

static const char *ota_chunk(const uint8_t *payload, size_t length) {
    if (!s_ota.active || s_ota.handle == 0) return "OTA not active";
    if (length <= 4) return "OTA chunk empty";
    const size_t offset = ((size_t)payload[0] << 24) | ((size_t)payload[1] << 16) |
                          ((size_t)payload[2] << 8) | payload[3];
    const size_t chunk_size = length - 4;
    if (offset != s_ota.received) return "OTA offset mismatch";
    if (s_ota.received + chunk_size > s_ota.expected_size) {
        ota_abort();
        return "OTA exceeds declared size";
    }
    if (esp_ota_write(s_ota.handle, payload + 4, chunk_size) != ESP_OK ||
        mbedtls_sha256_update(&s_ota.sha, payload + 4, chunk_size) != 0) {
        ota_abort();
        return "flash write failed";
    }
    s_ota.received += chunk_size;
    return NULL;
}

static const char *ota_end(void) {
    if (!s_ota.active || s_ota.handle == 0) return "OTA not active";
    if (s_ota.received != s_ota.expected_size) return "firmware incomplete";
    uint8_t digest[32];
    if (mbedtls_sha256_finish(&s_ota.sha, digest) != 0 ||
        memcmp(digest, s_ota.expected_sha, sizeof(digest)) != 0) {
        ota_abort();
        return "firmware SHA-256 mismatch";
    }

    const esp_ota_handle_t handle = s_ota.handle;
    const esp_partition_t *partition = s_ota.partition;
    char version[sizeof(s_ota.version)];
    memcpy(version, s_ota.version, sizeof(version));
    s_ota.handle = 0;
    s_ota.active = false;
    esp_err_t err = esp_ota_end(handle);
    esp_app_desc_t installed = {0};
    if (err == ESP_OK) err = esp_ota_get_partition_description(partition, &installed);
    if (err == ESP_OK && strcmp(installed.version, version) != 0) {
        ESP_LOGE(TAG, "Signed metadata version %s does not match image version %s",
                 version, installed.version);
        err = ESP_ERR_INVALID_ARG;
    }
    if (err == ESP_OK && !set_minimum_version(version)) err = ESP_FAIL;
    if (err == ESP_OK && !set_recovery_requested(false)) err = ESP_FAIL;
    if (err == ESP_OK) err = esp_ota_set_boot_partition(partition);
    ota_abort();
    return err == ESP_OK ? NULL : "ESP image validation failed";
}

static void notify_device_status(uint8_t sequence) {
    const esp_partition_t *partition = main_partition();
    char json[230];
    snprintf(json, sizeof(json),
             "{\"id\":\"%s\",\"name\":\"%s\",\"fw\":\"%s\","
             "\"board\":\"%s\",\"ota\":true,\"max\":%u,\"secure\":true,"
             "\"slot\":\"recovery\",\"update\":\"recovery\",\"role\":\"recovery\"}",
             s_device_id, s_device_name, esp_app_get_description()->version,
             RECOVERY_BOARD_NAME, partition == NULL ? 0U : (unsigned)partition->size);
    notify_json(PACKET_DEVICE_STATUS, sequence, json);
}

static int gatt_access(uint16_t connection_handle, uint16_t attribute_handle,
                       struct ble_gatt_access_ctxt *context, void *argument) {
    (void)connection_handle;
    (void)attribute_handle;
    (void)argument;
    if (context == NULL || context->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    const uint16_t packet_size = OS_MBUF_PKTLEN(context->om);
    if (packet_size < PROTOCOL_HEADER_SIZE || packet_size > CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    uint8_t packet[CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU];
    uint16_t copied = 0;
    if (ble_hs_mbuf_to_flat(context->om, packet, sizeof(packet), &copied) != 0 ||
        copied != packet_size) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    if (packet[0] != PROTOCOL_MAGIC || packet[1] != PROTOCOL_VERSION ||
        packet[4] != 0 || packet[5] != 1) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    const size_t payload_size = ((size_t)packet[6] << 8) | packet[7];
    if (payload_size != packet_size - PROTOCOL_HEADER_SIZE) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    const uint8_t type = packet[2];
    const uint8_t sequence = packet[3];
    const uint8_t *payload = packet + PROTOCOL_HEADER_SIZE;
    const char *error = NULL;
    switch (type) {
        case PACKET_DEVICE_STATUS:
            notify_device_status(sequence);
            break;
        case PACKET_OTA_BEGIN:
            error = ota_begin(payload, payload_size);
            notify_ack(sequence, error == NULL, error, false);
            break;
        case PACKET_OTA_CHUNK:
            error = ota_chunk(payload, payload_size);
            if (error != NULL) notify_ack(sequence, false, error, false);
            break;
        case PACKET_OTA_END:
            error = ota_end();
            notify_ack(sequence, error == NULL, error, error == NULL);
            if (error == NULL) schedule_restart(1000);
            break;
        case PACKET_OTA_ABORT:
            ota_abort();
            notify_ack(sequence, true, NULL, false);
            break;
        default:
            notify_ack(sequence, false, "unsupported in recovery", false);
            break;
    }
    return 0;
}

static const struct ble_gatt_chr_def k_characteristics[] = {
    {
        .uuid = &k_rx_uuid.u,
        .access_cb = gatt_access,
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_ENC,
        .min_key_size = 16,
    },
    {
        .uuid = &k_tx_uuid.u,
        .access_cb = gatt_access,
        .flags = BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &s_tx_value_handle,
    },
    {0},
};

static const struct ble_gatt_svc_def k_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &k_service_uuid.u,
        .characteristics = k_characteristics,
    },
    {0},
};

static void advertise(void) {
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = (ble_uuid128_t *)&k_service_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    if (ble_gap_adv_set_fields(&fields) != 0) return;

    struct ble_hs_adv_fields response = {0};
    response.name = (uint8_t *)s_device_name;
    response.name_len = strlen(s_device_name);
    response.name_is_complete = 1;
    if (ble_gap_adv_rsp_set_fields(&response) != 0) return;

    uint8_t address_type = 0;
    if (ble_hs_id_infer_auto(0, &address_type) != 0) return;
    struct ble_gap_adv_params parameters = {0};
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
    parameters.itvl_min = 24;
    parameters.itvl_max = 40;
    ble_gap_adv_start(address_type, NULL, BLE_HS_FOREVER, &parameters, gap_event, NULL);
}

static int gap_event(struct ble_gap_event *event, void *argument) {
    (void)argument;
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                s_connection_handle = event->connect.conn_handle;
                s_notify_subscribed = false;
                s_link_encrypted = false;
                ble_gap_security_initiate(s_connection_handle);
            } else {
                advertise();
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            s_connection_handle = BLE_HS_CONN_HANDLE_NONE;
            s_notify_subscribed = false;
            s_link_encrypted = false;
            ota_abort();
            advertise();
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == s_tx_value_handle) {
                s_notify_subscribed = event->subscribe.cur_notify != 0;
                if (s_notify_subscribed && s_link_encrypted) notify_json(PACKET_ACK, 0, "{\"r\":true}");
            }
            break;
        case BLE_GAP_EVENT_ENC_CHANGE:
            s_link_encrypted = event->enc_change.status == 0;
            if (s_link_encrypted && s_notify_subscribed) notify_json(PACKET_ACK, 0, "{\"r\":true}");
            break;
        case BLE_GAP_EVENT_REPEAT_PAIRING: {
            struct ble_gap_conn_desc description = {0};
            if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &description) == 0) {
                ble_store_util_delete_peer(&description.peer_id_addr);
                return BLE_GAP_REPEAT_PAIRING_RETRY;
            }
            return BLE_GAP_REPEAT_PAIRING_IGNORE;
        }
        case BLE_GAP_EVENT_ADV_COMPLETE:
            advertise();
            break;
        default:
            break;
    }
    return 0;
}

static void on_sync(void) {
    advertise();
}

static void on_reset(int reason) {
    ESP_LOGE(TAG, "NimBLE reset: %d", reason);
    s_connection_handle = BLE_HS_CONN_HANDLE_NONE;
    s_notify_subscribed = false;
    s_link_encrypted = false;
    ota_abort();
}

static void host_task(void *argument) {
    (void)argument;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void build_identity(void) {
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
        snprintf(s_device_id, sizeof(s_device_id), "CH-%02X%02X", mac[4], mac[5]);
        snprintf(s_device_name, sizeof(s_device_name), "Chronchi-R%02X%02X", mac[4], mac[5]);
    }
}

static bool recovery_button_held(void) {
    gpio_config_t config = {
        .pin_bit_mask = 1ULL << RECOVERY_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&config);
    vTaskDelay(pdMS_TO_TICKS(30));
    return gpio_get_level(RECOVERY_BUTTON) == 0;
}

void app_main(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    mbedtls_sha256_init(&s_ota.sha);

    const bool requested = recovery_requested();
    const bool button_held = recovery_button_held();
    const esp_partition_t *main = main_partition();
    if (!requested && !button_held && main != NULL) {
        esp_app_desc_t installed = {0};
        if (esp_ota_get_partition_description(main, &installed) == ESP_OK &&
            set_minimum_version(installed.version) &&
            esp_ota_set_boot_partition(main) == ESP_OK) {
            ESP_LOGI(TAG, "Valid main image %s found; leaving factory recovery",
                     installed.version);
            esp_restart();
        }
        ESP_LOGE(TAG, "Main image or anti-rollback state is invalid; staying in recovery");
    }

    build_identity();
    ESP_LOGW(TAG, "BLE recovery active (requested=%d, button=%d)", requested, button_held);
    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_store_config_init();
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ESP_ERROR_CHECK(ble_gatts_count_cfg(k_services) == 0 ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(ble_gatts_add_svcs(k_services) == 0 ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(ble_svc_gap_device_name_set(s_device_name) == 0 ? ESP_OK : ESP_FAIL);
    nimble_port_freertos_init(host_task);
}
