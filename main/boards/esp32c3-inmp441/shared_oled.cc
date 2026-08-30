#include "shared_oled.h"

#include "config.h"
#include "display/oled_display.h"
#include "settings.h"

#include <driver/i2c_master.h>
#include <esp_err.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_ssd1306.h>
#include <esp_log.h>

namespace {
constexpr char kTag[] = "SharedOled";
}  // namespace

Display* GetEsp32c3SharedOled(bool chronchi_mode) {
    static Display* display = nullptr;
    static i2c_master_bus_handle_t i2c_bus = nullptr;
    static NoDisplay no_display;
    if (display != nullptr) {
        return display;
    }

    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = DISPLAY_SDA_PIN;
    bus_config.scl_io_num = DISPLAY_SCL_PIN;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;
    esp_err_t err = i2c_new_master_bus(&bus_config, &i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "I2C init failed: %s", esp_err_to_name(err));
        display = &no_display;
        return display;
    }

    bool animated_mode = false;
    if (!chronchi_mode) {
        Settings settings("display", false);
        animated_mode = settings.GetString("oled_mode", "classic") == "amoled";
    }
    ESP_LOGI(kTag, "OLED owner=%s layout=%s SDA=%d SCL=%d",
             chronchi_mode ? "Chronchi" : "Xiaozhi",
             chronchi_mode ? "dynamic-128x64" : (animated_mode ? "animated" : "classic"),
             DISPLAY_SDA_PIN, DISPLAY_SCL_PIN);

    const uint8_t addresses[] = {0x3c, 0x3d};
    for (uint8_t address : addresses) {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        ESP_LOGI(kTag, "Probing SSD1306 at 0x%02x", address);

        esp_lcd_panel_io_i2c_config_t io_config = {};
        io_config.dev_addr = address;
        io_config.control_phase_bytes = 1;
        io_config.dc_bit_offset = 6;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        io_config.scl_speed_hz = 400000;
        if (esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &panel_io) != ESP_OK) {
            continue;
        }

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.bits_per_pixel = 1;
        if (esp_lcd_new_panel_ssd1306(panel_io, &panel_config, &panel) != ESP_OK) {
            esp_lcd_panel_io_del(panel_io);
            continue;
        }
        err = esp_lcd_panel_reset(panel);
        if (err == ESP_OK) err = esp_lcd_panel_init(panel);
        if (err == ESP_OK) err = esp_lcd_panel_disp_on_off(panel, true);
        if (err != ESP_OK) {
            esp_lcd_panel_del(panel);
            esp_lcd_panel_io_del(panel_io);
            continue;
        }

        ESP_LOGI(kTag, "SSD1306 ready at 0x%02x", address);
        display = new OledDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                  DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                  animated_mode, chronchi_mode);
        return display;
    }

    ESP_LOGW(kTag, "OLED not found at 0x3c/0x3d; continuing headless");
    display = &no_display;
    return display;
}
