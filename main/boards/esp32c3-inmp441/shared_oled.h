#ifndef ESP32C3_INMP441_SHARED_OLED_H_
#define ESP32C3_INMP441_SHARED_OLED_H_

class Display;

// The only SSD1306/I2C construction path used by both boot modes.
Display* GetEsp32c3SharedOled(bool chronchi_mode = false);

#endif  // ESP32C3_INMP441_SHARED_OLED_H_
