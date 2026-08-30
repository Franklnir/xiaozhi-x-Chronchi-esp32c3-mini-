#include "mode_store.h"

#include <esp_log.h>
#include <nvs.h>

namespace {
constexpr char kNamespace[] = "system_mode";
constexpr char kKey[] = "boot_mode";
constexpr char kTag[] = "ModeManager";

BootMode DefaultMode() {
#if CONFIG_CHRONCHI_DEFAULT_MODE
    return BootMode::Chronchi;
#else
    return BootMode::Xiaozhi;
#endif
}
}  // namespace

BootMode ModeStore::Load() {
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(kTag, "No saved boot mode; defaulting to %s", BootModeName(DefaultMode()));
        return DefaultMode();
    }
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Cannot open mode namespace (%s); defaulting to %s",
                 esp_err_to_name(err), BootModeName(DefaultMode()));
        return DefaultMode();
    }

    uint8_t raw = 0;
    err = nvs_get_u8(handle, kKey, &raw);
    nvs_close(handle);
    if (err != ESP_OK || raw > static_cast<uint8_t>(BootMode::Chronchi)) {
        ESP_LOGW(kTag, "Missing/invalid boot mode (%s, value=%u); defaulting to %s",
                 esp_err_to_name(err), static_cast<unsigned>(raw),
                 BootModeName(DefaultMode()));
        return DefaultMode();
    }
    const BootMode requested_mode = static_cast<BootMode>(raw);
    if (requested_mode == BootMode::Chronchi) {
        // Chronchi is a one-shot boot mode. Consume its boot ticket before
        // starting Chronchi so every later reset or power cycle returns to Xiaozhi.
        const esp_err_t clear_err = Save(BootMode::Xiaozhi);
        if (clear_err != ESP_OK) {
            ESP_LOGE(kTag, "Cannot consume Chronchi one-shot ticket (%s); booting Xiaozhi for safety",
                     esp_err_to_name(clear_err));
            return BootMode::Xiaozhi;
        }
        ESP_LOGI(kTag, "Consumed Chronchi one-shot ticket; next boot will use Xiaozhi");
    }
    return requested_mode;
}

esp_err_t ModeStore::Save(BootMode mode) {
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u8(handle, kKey, static_cast<uint8_t>(mode));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err == ESP_OK) {
        ESP_LOGI(kTag, "Saved boot mode: %s", BootModeName(mode));
    }
    return err;
}
