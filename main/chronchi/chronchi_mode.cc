#include "chronchi_mode.h"

#include "boards/esp32c3-inmp441/config.h"
#include "boards/esp32c3-inmp441/shared_oled.h"
#include "display/display.h"
#include "mode/mode_store.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
constexpr char kTag[] = "ChronchiMode";

void LogHeap(const char* stage) {
    ESP_LOGI(kTag, "Heap %s: free=%u largest=%u min=%u", stage,
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)));
}
}  // namespace

ChronchiMode::ChronchiMode() : ble_(state_), button_(BOOT_BUTTON_GPIO) {}

void ChronchiMode::Initialize() {
    ESP_LOGI(kTag, "Radio policy: Chronchi owns BLE; Wi-Fi initialization is forbidden");
    ESP_LOGI(kTag, "ESPBridge BLE V1 framed JSON mode; upstream Chronos protocol is disabled");
    LogHeap("before OLED/BLE");

    // Battery monitor removed — not displayed on OLED

    display_ = GetEsp32c3SharedOled(true);
    mode_selector_ = std::make_unique<ModeSelector>(
        BootMode::Chronchi, display_, [this]() {
            if (!ble_.ConfirmPairing()) state_.DismissTransient();
        });
    button_.OnPressDown([this]() {
        mode_selector_->OnPressDown();

        // Triple-click detection for clearing WiFi
        int64_t now = esp_timer_get_time();
        if (now - last_click_us_ < kTripleClickWindowUs) {
            click_count_++;
        } else {
            click_count_ = 1;
        }
        last_click_us_ = now;

        if (click_count_ >= 3) {
            click_count_ = 0;
            ESP_LOGI(kTag, "Triple-click detected! Clearing WiFi config...");
            nvs_handle_t handle;
            esp_err_t err = nvs_open("wifi", NVS_READWRITE, &handle);
            if (err == ESP_OK) {
                nvs_erase_all(handle);
                nvs_commit(handle);
                nvs_close(handle);
                ESP_LOGI(kTag, "WiFi config cleared from NVS");
                state_.ShowSystem("WIFI", "Cleared!");
                Render(state_.Snapshot());
            }
        }
    });
    button_.OnPressUp([this]() { mode_selector_->OnPressUp(); });

    state_.ShowStartup();
    Render(state_.Snapshot());
    ble_status_ = ble_.Initialize();
    if (ble_status_ != ESP_OK) {
        ESP_LOGE(kTag, "BLE unavailable: %s; mode button remains operational",
                 esp_err_to_name(ble_status_));
        state_.ShowSystem("BLE init failed", esp_err_to_name(ble_status_));
        Render(state_.Snapshot());
    }

    // Firebase config callback removed (Xichi mode deleted)
    ble_.GetProtocol().on_firebase_config_ = [this](const FirebaseConfigData& config) {
        ESP_LOGW(kTag, "Firebase config received but Xichi mode is removed, ignoring");
        state_.ShowSystem("CONFIG", "Ignored (no Xichi)");
        Render(state_.Snapshot());
    };

    // Clear config callback removed (Xichi mode deleted)
    ble_.GetProtocol().on_clear_config_ = [this]() {
        ESP_LOGW(kTag, "Clear config received but Xichi mode is removed, ignoring");
    };

    // Register WiFi config callback from BLE
    ble_.GetProtocol().on_wifi_config_ = [this](const WifiConfigData& wifi_config) {
        ESP_LOGI(kTag, "WiFi config received via BLE: ssid=%s", wifi_config.ssid);
        nvs_handle_t handle;
        esp_err_t err = nvs_open("wifi", NVS_READWRITE, &handle);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "NVS open failed: %s", esp_err_to_name(err));
            state_.ShowSystem("WIFI", "Save failed!");
            Render(state_.Snapshot());
            return;
        }
        nvs_set_str(handle, "ssid", wifi_config.ssid);
        nvs_set_str(handle, "password", wifi_config.password);
        nvs_set_u8(handle, "configured", 1);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(kTag, "WiFi config saved to NVS: %s", wifi_config.ssid);
        state_.ShowSystem("WIFI", "Saved!");
        Render(state_.Snapshot());
    };

    // Register switch mode callback from BLE
    ble_.GetProtocol().on_switch_mode_ = [this](const char* mode) {
        ESP_LOGI(kTag, "Switch mode command: %s", mode);
        if (strcmp(mode, "xiaozhi") == 0) {
            ModeStore::Save(BootMode::Xiaozhi);
            state_.ShowSystem("SWITCHING", "Xiaozhi mode...");
            Render(state_.Snapshot());
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        } else if (strcmp(mode, "chronchi") == 0) {
            ESP_LOGI(kTag, "Already in Chronchi mode");
        }
    };

    // Register WiFi scan callback from BLE
    ble_.GetProtocol().on_wifi_scan_ = [this]() {
        ESP_LOGI(kTag, "WiFi scan requested via BLE");
        // TODO: Implement WiFi scan and send results back via BLE
        state_.ShowSystem("WIFI", "Scanning...");
        Render(state_.Snapshot());
    };

    // Firebase status callback (Xichi mode removed, always "not saved")
    ble_.GetProtocol().on_firebase_status_ = [this]() {
        ESP_LOGI(kTag, "Firebase status requested via BLE — Xichi removed, no config");
        state_.ShowSystem("FIREBASE", "Not available");
        Render(state_.Snapshot());
    };

    LogHeap("after OLED/BLE");

#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state = ESP_OTA_IMG_UNDEFINED;
    if (running != nullptr && esp_ota_get_state_partition(running, &ota_state) == ESP_OK &&
        ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        rollback_confirmation_due_us_ = esp_timer_get_time() + 10 * 1000 * 1000;
        ESP_LOGI(kTag, "New OTA image pending validation; stability window started");
    }
#endif
}

void ChronchiMode::Render(const ChronchiSnapshot& snapshot) {
    if (display_ == nullptr || mode_selector_ == nullptr || mode_selector_->IsMenuActive()) return;
    display_->SetChronchiScreen(snapshot.active);
}

[[noreturn]] void ChronchiMode::Run() {
    uint32_t last_revision = UINT32_MAX;
    while (true) {
        if (ble_status_ == ESP_OK) ble_.Poll();
#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
        if (rollback_confirmation_due_us_ != 0 &&
            esp_timer_get_time() >= rollback_confirmation_due_us_) {
            const esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
            ESP_LOGI(kTag, "OTA image validation result: %s", esp_err_to_name(result));
            rollback_confirmation_due_us_ = 0;
        }
#endif
        state_.Tick();
        const ChronchiSnapshot snapshot = state_.Snapshot();
        if (snapshot.revision != last_revision) {
            Render(snapshot);
            last_revision = snapshot.revision;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
