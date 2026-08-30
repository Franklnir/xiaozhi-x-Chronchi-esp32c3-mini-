#include "sleep_manager.h"
#include <esp_log.h>
#include <driver/gpio.h>

#define TAG "SleepManager"

SleepManager::SleepManager() {
    last_activity_us_ = esp_timer_get_time();
}

SleepManager::~SleepManager() {}

bool SleepManager::Initialize() {
    ConfigureWakeSources();
    ESP_LOGI(TAG, "Sleep manager initialized");
    return true;
}

void SleepManager::SetEnabled(bool enabled) {
    enabled_ = enabled;
    ESP_LOGI(TAG, "Sleep mode %s", enabled ? "enabled" : "disabled");
}

void SleepManager::SetTimeout(int seconds) {
    timeout_sec_ = seconds;
    ESP_LOGI(TAG, "Sleep timeout set to %d seconds", seconds);
}

void SleepManager::ResetActivity() {
    last_activity_us_ = esp_timer_get_time();
}

void SleepManager::CheckAndSleep() {
    if (!enabled_) return;
    
    int64_t now = esp_timer_get_time();
    int64_t idle_us = now - last_activity_us_;
    int64_t timeout_us = (int64_t)timeout_sec_ * 1000000LL;
    
    if (idle_us >= timeout_us) {
        ESP_LOGI(TAG, "Idle for %lld seconds, entering sleep", idle_us / 1000000);
        EnterLightSleep();
    }
}

void SleepManager::EnterLightSleep() {
    ESP_LOGI(TAG, "Entering light sleep...");
    
    // Configure sleep time
    esp_sleep_enable_timer_wakeup((uint64_t)timeout_sec_ * 1000000ULL);
    
    // Enter light sleep
    esp_light_sleep_start();
    
    // Wake up here
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    
    switch (cause) {
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "Woke up from timer");
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            ESP_LOGI(TAG, "Woke up from GPIO");
            break;
        default:
            ESP_LOGI(TAG, "Woke up, cause: %d", cause);
            break;
    }
    
    // Reset activity timer
    ResetActivity();
}

void SleepManager::ConfigureWakeSources() {
    if (configured_) return;

    // ESP32-C3 light sleep uses the GPIO wake-up API. EXT0 is not available
    // on this target in ESP-IDF 5.5.
    ESP_ERROR_CHECK(gpio_wakeup_enable(GPIO_NUM_3, GPIO_INTR_LOW_LEVEL));
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());
    
    configured_ = true;
    ESP_LOGI(TAG, "Wake sources configured");
}
