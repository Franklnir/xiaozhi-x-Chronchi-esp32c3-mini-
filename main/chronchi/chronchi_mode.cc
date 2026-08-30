#include "chronchi_mode.h"

#include "boards/esp32c3-inmp441/config.h"
#include "boards/esp32c3-inmp441/shared_oled.h"
#include "display/display.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
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

    battery_monitor_ = std::make_unique<AdcBatteryMonitor>(
        BATTERY_ADC_UNIT, BATTERY_ADC_CHANNEL,
        BATTERY_DIVIDER_UPPER_RESISTOR_OHM,
        BATTERY_DIVIDER_LOWER_RESISTOR_OHM);
    ESP_LOGI(kTag, "Battery ADC: GPIO %d, divider %.0f/%.0f ohm",
             BATTERY_ADC_GPIO,
             BATTERY_DIVIDER_UPPER_RESISTOR_OHM,
             BATTERY_DIVIDER_LOWER_RESISTOR_OHM);
    UpdateBattery(true);

    display_ = GetEsp32c3SharedOled(true);
    mode_selector_ = std::make_unique<ModeSelector>(
        BootMode::Chronchi, display_, [this]() {
            if (!ble_.ConfirmPairing()) state_.DismissTransient();
        });
    button_.OnPressDown([this]() { mode_selector_->OnPressDown(); });
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

void ChronchiMode::UpdateBattery(bool force) {
    if (battery_monitor_ == nullptr) return;

    const int64_t now_us = esp_timer_get_time();
    if (!force && now_us < battery_refresh_due_us_) return;
    battery_refresh_due_us_ = now_us +
        static_cast<int64_t>(BATTERY_REFRESH_INTERVAL_MS) * 1000;

    uint8_t level = 0;
    if (!battery_monitor_->ReadBatteryLevel(level)) {
        ESP_LOGW(kTag, "Battery ADC sample is not valid yet");
        return;
    }
    state_.UpdateDeviceBattery(level, battery_monitor_->IsCharging());
}

[[noreturn]] void ChronchiMode::Run() {
    uint32_t last_revision = UINT32_MAX;
    while (true) {
        if (ble_status_ == ESP_OK) ble_.Poll();
        UpdateBattery();
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
