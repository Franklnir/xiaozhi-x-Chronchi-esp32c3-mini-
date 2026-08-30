#include <esp_log.h>
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application.h"
#include "chronchi/chronchi_mode.h"
#include "mode/mode_store.h"
#include "system_info.h"

#define TAG "ModeManager"

extern "C" void app_main(void)
{
    // Initialize NVS flash for WiFi configuration
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS flash to fix corruption");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    const BootMode boot_mode = ModeStore::Load();
    ESP_LOGI(TAG, "Selected boot mode: %s (%u)", BootModeName(boot_mode),
             static_cast<unsigned>(boot_mode));

#if CONFIG_ENABLE_CHRONCHI_MODE
    if (boot_mode == BootMode::Chronchi) {
        // Deliberately do not construct Board/Application: no Wi-Fi/audio/wake-word tasks.
        static ChronchiMode chronchi;
        chronchi.Initialize();
        chronchi.Run();
    }
#else
    if (boot_mode == BootMode::Chronchi) {
        ESP_LOGE(TAG, "Chronchi was selected but is not compiled; falling back to Xiaozhi");
    }
#endif

    ESP_LOGI(TAG, "Radio policy: Xiaozhi owns Wi-Fi; Chronchi BLE is not initialized");
    auto& app = Application::GetInstance();
    app.Initialize();
    app.Run();  // This function runs the main event loop and never returns
}
