#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// Audio Configuration
// INMP441 Mic + MAX98357A Speaker share the same I2S clock, use same sample rate
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// I2S Configuration - Duplex mode with shared clocks
// ESP32-C3 has only ONE I2S peripheral, mic and speaker share BCLK/WS
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_6   // Word Select (shared)
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_5   // Bit Clock (shared)
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_4   // INMP441 SD (Mic Data In)
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_7   // MAX98357A DIN (Speaker Data Out)

// Button Configuration
#define BOOT_BUTTON_GPIO    GPIO_NUM_3   // Short click: chat; hold 2.5s: boot-mode menu
#define RESET_SSID_BUTTON_GPIO GPIO_NUM_2 // Click: hands-free toggle, hold: clear saved WiFi SSID
#define RESET_SSID_LONG_PRESS_MS 5000     // Long press duration in milliseconds

// Device battery measurement (single-cell Li-ion, 100k/100k divider).
// BAT+ -> 100k -> GPIO1 -> 100k -> GND, with 100nF from GPIO1 to GND.
#define BATTERY_ADC_GPIO                    GPIO_NUM_1
#define BATTERY_ADC_UNIT                    ADC_UNIT_1
#define BATTERY_ADC_CHANNEL                 ADC_CHANNEL_1
#define BATTERY_DIVIDER_UPPER_RESISTOR_OHM  100000.0f
#define BATTERY_DIVIDER_LOWER_RESISTOR_OHM  100000.0f
#define BATTERY_REFRESH_INTERVAL_MS         5000

// Hands-free mode: auto enter listen mode when device is idle.
// Set to 0 to restore pure push-to-talk behavior on GPIO3.
#define HANDS_FREE_AUTO_LISTEN              1
#define HANDS_FREE_AUTO_LISTEN_INTERVAL_MS  500
#define HANDS_FREE_AUTO_LISTEN_RETRY_MS     5000
#define HANDS_FREE_IDLE_TIMEOUT_MS          30000

// LED Configuration (optional - set to NC if not used)
#define BUILTIN_LED_GPIO    GPIO_NUM_NC  // No built-in LED

// OLED Display Configuration (SSD1306 128x64 via I2C)
#define DISPLAY_SDA_PIN     GPIO_NUM_8   // I2C SDA
#define DISPLAY_SCL_PIN     GPIO_NUM_9   // I2C SCL
#define DISPLAY_WIDTH       128
#define DISPLAY_HEIGHT      64
#define DISPLAY_MIRROR_X    true
#define DISPLAY_MIRROR_Y    true

#endif // _BOARD_CONFIG_H_
