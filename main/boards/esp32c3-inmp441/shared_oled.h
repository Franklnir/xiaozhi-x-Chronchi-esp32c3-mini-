#ifndef ESP32C3_INMP441_SHARED_OLED_H_
#define ESP32C3_INMP441_SHARED_OLED_H_

#include <driver/i2c_master.h>

class Display;

// Shared I2C master bus handle used by OLED and Audio Codec (ES8311)
i2c_master_bus_handle_t GetEsp32c3SharedI2cBus();

// The only SSD1306/I2C construction path used by both boot modes.
Display* GetEsp32c3SharedOled(bool chronchi_mode = false);

#endif  // ESP32C3_INMP441_SHARED_OLED_H_
