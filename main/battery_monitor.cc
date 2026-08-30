#include "battery_monitor.h"
#include <esp_log.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali_scheme.h>

#define TAG "BatteryMonitor"

BatteryMonitor::BatteryMonitor() {}
BatteryMonitor::~BatteryMonitor() { Deinitialize(); }

bool BatteryMonitor::Initialize(int adc_pin) {
    if (initialized_) return true;
    
    adc_pin_ = adc_pin;
    
    // Initialize ADC
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t err = adc_oneshot_new_unit(&init_config, &adc_handle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init ADC: %s", esp_err_to_name(err));
        return false;
    }
    
    // Configure ADC channel
    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_12,
    };
    err = adc_oneshot_config_channel(adc_handle_, (adc_channel_t)adc_pin_, &chan_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config ADC channel: %s", esp_err_to_name(err));
        return false;
    }
    
    // Initialize calibration
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_12,
    };
    err = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration failed, using raw values");
        cali_handle_ = nullptr;
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "Battery monitor initialized on ADC pin %d", adc_pin);
    
    // Initial reading
    Update();
    
    return true;
}

void BatteryMonitor::Deinitialize() {
    if (cali_handle_) {
        adc_cali_delete_scheme_curve_fitting(cali_handle_);
        cali_handle_ = nullptr;
    }
    if (adc_handle_) {
        adc_oneshot_del_unit(adc_handle_);
        adc_handle_ = nullptr;
    }
    initialized_ = false;
}

int BatteryMonitor::ReadAdc() {
    if (!adc_handle_) return 0;
    
    int raw = 0;
    // Average multiple readings
    for (int i = 0; i < 10; i++) {
        int val = 0;
        adc_oneshot_read(adc_handle_, (adc_channel_t)adc_pin_, &val);
        raw += val;
    }
    return raw / 10;
}

int BatteryMonitor::GetVoltageMv() {
    if (!initialized_) return 0;
    
    int raw = ReadAdc();
    
    if (cali_handle_) {
        int voltage = 0;
        adc_cali_raw_to_voltage(cali_handle_, raw, &voltage);
        // Multiply by 2 if using voltage divider
        voltage_mv_ = voltage * 2;
    } else {
        // Rough conversion: 0-4095 -> 0-3300mV, then *2 for voltage divider
        voltage_mv_ = (raw * 3300 * 2) / 4095;
    }
    
    return voltage_mv_;
}

int BatteryMonitor::GetPercentage() {
    int voltage = GetVoltageMv();
    
    if (voltage <= voltage_empty_) return 0;
    if (voltage >= voltage_full_) return 100;
    
    percentage_ = ((voltage - voltage_empty_) * 100) / (voltage_full_ - voltage_empty_);
    return percentage_;
}

BatteryLevel BatteryMonitor::GetLevel() {
    int pct = GetPercentage();
    
    if (pct <= 0) return BATTERY_EMPTY;
    if (pct <= 25) return BATTERY_QUARTER;
    if (pct <= 50) return BATTERY_HALF;
    if (pct <= 75) return BATTERY_THREE_QUARTERS;
    return BATTERY_FULL;
}

bool BatteryMonitor::IsCharging() {
    // TODO: Implement charging detection if hardware supports it
    return false;
}

void BatteryMonitor::Update() {
    GetVoltageMv();
    GetPercentage();
}
