#include "adc_battery_monitor.h"

#include <algorithm>
#include <cmath>

AdcBatteryMonitor::AdcBatteryMonitor(adc_unit_t adc_unit, adc_channel_t adc_channel,
                                     float upper_resistor, float lower_resistor,
                                     gpio_num_t charging_pin)
    : charging_pin_(charging_pin) {
    if (charging_pin_ != GPIO_NUM_NC) {
        gpio_config_t gpio_cfg = {
            .pin_bit_mask = 1ULL << charging_pin_,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&gpio_cfg));
    }

    adc_battery_estimation_t adc_cfg = {
        .internal = {
            .adc_unit = adc_unit,
            .adc_bitwidth = ADC_BITWIDTH_DEFAULT,
            .adc_atten = ADC_ATTEN_DB_12,
        },
        .adc_channel = adc_channel,
        .upper_resistor = upper_resistor,
        .lower_resistor = lower_resistor,
    };

    if (charging_pin_ != GPIO_NUM_NC) {
        adc_cfg.charging_detect_cb = [](void* user_data) -> bool {
            auto* self = static_cast<AdcBatteryMonitor*>(user_data);
            return gpio_get_level(self->charging_pin_) == 1;
        };
        adc_cfg.charging_detect_user_data = this;
    } else {
        // With no charger-status pin, estimate charging from voltage trend.
        adc_cfg.charging_detect_cb = nullptr;
        adc_cfg.charging_detect_user_data = nullptr;
    }
    adc_battery_estimation_handle_ = adc_battery_estimation_create(&adc_cfg);

    esp_timer_create_args_t timer_cfg = {
        .callback = [](void* arg) {
            static_cast<AdcBatteryMonitor*>(arg)->CheckBatteryStatus();
        },
        .arg = this,
        .name = "adc_battery_monitor",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_cfg, &timer_handle_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, 1000000));
}

AdcBatteryMonitor::~AdcBatteryMonitor() {
    if (timer_handle_ != nullptr) {
        esp_timer_stop(timer_handle_);
        esp_timer_delete(timer_handle_);
    }
    if (adc_battery_estimation_handle_ != nullptr) {
        ESP_ERROR_CHECK(adc_battery_estimation_destroy(adc_battery_estimation_handle_));
    }
}

bool AdcBatteryMonitor::IsCharging() {
    if (adc_battery_estimation_handle_ != nullptr) {
        bool is_charging = false;
        if (adc_battery_estimation_get_charging_state(adc_battery_estimation_handle_,
                                                       &is_charging) == ESP_OK) {
            return is_charging;
        }
    }

    if (charging_pin_ != GPIO_NUM_NC) {
        return gpio_get_level(charging_pin_) == 1;
    }
    return false;
}

bool AdcBatteryMonitor::IsDischarging() {
    return !IsCharging();
}

bool AdcBatteryMonitor::ReadBatteryLevel(uint8_t& level) {
    if (adc_battery_estimation_handle_ == nullptr) {
        return false;
    }

    float capacity = 0.0f;
    if (adc_battery_estimation_get_capacity(adc_battery_estimation_handle_,
                                             &capacity) != ESP_OK ||
        !std::isfinite(capacity)) {
        return false;
    }

    capacity = std::clamp(capacity, 0.0f, 100.0f);
    level = static_cast<uint8_t>(std::lround(capacity));
    return true;
}

uint8_t AdcBatteryMonitor::GetBatteryLevel() {
    uint8_t level = 100;
    ReadBatteryLevel(level);
    return level;
}

void AdcBatteryMonitor::OnChargingStatusChanged(std::function<void(bool)> callback) {
    on_charging_status_changed_ = callback;
}

void AdcBatteryMonitor::CheckBatteryStatus() {
    const bool new_charging_status = IsCharging();
    if (new_charging_status != is_charging_) {
        is_charging_ = new_charging_status;
        if (on_charging_status_changed_) {
            on_charging_status_changed_(is_charging_);
        }
    }
}
