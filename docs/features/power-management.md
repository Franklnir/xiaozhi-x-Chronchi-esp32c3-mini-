# Feature: Power Management

---

## Overview

ESP32-C3 mendukung power management untuk menghemat daya baterai. Fitur ini meliputi battery monitoring, sleep modes, dan power save levels untuk Wi-Fi dan CPU.

---

## Battery Monitor

### Hardware

```
BAT+ ──── 100kΩ ────┬──── GPIO 1 (ADC)
                     │
                   100nF
                     │
                    GND
```

- **Voltage divider:** 100kΩ / 100kΩ (1:2 ratio)
- **ADC:** GPIO1, ADC_UNIT_1, CHANNEL_1
- **Cell:** Single-cell Li-ion (3.0V - 4.2V)

### Configuration

```c
// boards/esp32c3-inmp441/config.h
#define BATTERY_ADC_GPIO                    GPIO_NUM_1
#define BATTERY_ADC_UNIT                    ADC_UNIT_1
#define BATTERY_ADC_CHANNEL                 ADC_CHANNEL_1
#define BATTERY_DIVIDER_UPPER_RESISTOR_OHM  100000.0f
#define BATTERY_DIVIDER_LOWER_RESISTOR_OHM  100000.0f
#define BATTERY_REFRESH_INTERVAL_MS         5000  // 5 detik
```

### ADC Reading

```cpp
class AdcBatteryMonitor {
public:
    AdcBatteryMonitor(adc_unit_t unit, adc_channel_t channel,
                      float upper_resistor, float lower_resistor);
    
    bool ReadBatteryLevel(uint8_t& level);  // 0-100%
    bool IsCharging();                       // Deteksi charging
    float ReadVoltage();                     // Raw voltage
};
```

### Battery Level Calculation

```cpp
// ADC value → voltage → percentage
float raw_voltage = adc_reading * 3.3f / 4095.0f;
float battery_voltage = raw_voltage * (upper + lower) / lower;

// Li-ion voltage → percentage
// 4.2V = 100%, 3.7V = 50%, 3.0V = 0%
uint8_t level = map_voltage_to_percentage(battery_voltage);
```

### Charging Detection

Charging terdeteksi ketika:
- Voltage naik > 4.2V (charging active)
- Voltage stabil di ~4.2V (fully charged)

### Display

```
⚡85%  →  Battery 85%
⚡CHG  →  Charging
```

---

## Power Save Levels

### Levels

```cpp
enum class PowerSaveLevel {
    PERFORMANCE,  // Full speed, no power save
    LOW_POWER,    // Wi-Fi power save active
    SLEEP,        // CPU light sleep
};
```

### Wi-Fi Power Save

```cpp
// Enable Wi-Fi power save (hemat ~20mA)
esp_wifi_set_ps(WIFI_PS_MODEM);

// Disable Wi-Fi power save (full speed)
esp_wifi_set_ps(WIFI_PS_NONE);
```

**Impact:**
- Modem sleep: Wi-Fi radio mati saat idle, bangun saat ada data
- Hemat ~20mA saat idle
- Latency naik ~10ms saat pertama kirim data

### CPU Power Save

```cpp
// Enable automatic light sleep
esp_pm_configure(&pm_config);

// Configuration
esp_pm_config_esp32c3_t pm_config = {
    .max_freq_mhz = 160,
    .min_freq_mhz = 80,
    .light_sleep_enable = true
};
```

---

## Audio Power Management

### Audio Power Timer

```cpp
// Matikan mic/speaker saat idle selama 30 detik
#define AUDIO_POWER_TIMEOUT_MS 30000
#define AUDIO_POWER_CHECK_INTERVAL_MS 500
```

### Behavior

```
Audio idle 30 detik → matikan mic & speaker → hemat power
Audio aktif → nyalakan mic & speaker → full power
```

### Implementation

```cpp
void AudioService::CheckAndUpdateAudioPowerState() {
    auto now = std::chrono::steady_clock::now();
    
    auto input_idle = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_input_time_).count();
    auto output_idle = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_output_time_).count();
    
    if (input_idle > AUDIO_POWER_TIMEOUT_MS && codec_->input_enabled()) {
        codec_->EnableInput(false);
    }
    
    if (output_idle > AUDIO_POWER_TIMEOUT_MS && codec_->output_enabled()) {
        codec_->EnableOutput(false);
    }
}
```

---

## Sleep Modes

### Active Mode

- CPU: Running
- Wi-Fi: Active
- BLE: Active
- Power: ~80-120mA

### Modem Sleep

- CPU: Running
- Wi-Fi: Sleep between DTIM
- BLE: Active
- Power: ~30-50mA

### Light Sleep

- CPU: Suspended (wake on timer/interrupt)
- Wi-Fi: Sleep
- BLE: Sleep
- Power: ~1-5mA

### Deep Sleep

- CPU: Off
- Wi-Fi: Off
- BLE: Off
- RTC: Active
- Power: ~10µA

**Catatan:** Deep sleep TIDAK digunakan karena device perlu tetap terima BLE/Wi-Fi.

---

## Mode-Specific Power

### Xiaozhi Mode

```
Idle (standby):     ~30-50mA (Wi-Fi modem sleep)
Listening (mic):    ~80-100mA (full speed)
Speaking (TTS):     ~60-80mA (speaker active)
Wake word detect:   ~50-70mA (mic + Wi-Fi)
```

### Chronchi Mode

```
BLE advertising:    ~10-15mA (BLE only)
BLE connected:      ~15-20mA (BLE active)
Display update:     ~20-25mA (OLED + BLE)
```

### Xichi Mode

```
Standby:            ~30-50mA (Wi-Fi + BLE)
Open channel:       ~40-60mA (Wi-Fi active + BLE)
Speaking:           ~60-80mA (speaker + Wi-Fi)
```

---

## Power Optimization Tips

### 1. Disable Wake Word (Xichi mode)

```cpp
// Hemat ~15-20KB RAM dan ~10mA
audio_service_.EnableWakeWordDetection(false);
```

### 2. Wi-Fi Buffer Minimum

```c
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=3
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=6
```

Hemat ~30KB RAM.

### 3. BLE Minimal Config

```c
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1
CONFIG_BT_NIMBLE_ROLE_CENTRAL=n
CONFIG_BT_NIMBLE_ROLE_OBSERVER=n
```

Hemat ~20KB RAM.

### 4. Display Power Save

```cpp
// Matikan display saat idle lama
display_->SetPowerSaveMode(true);
```

Hemat ~5mA.

### 5. Audio Power Timer

```cpp
// Matikan mic/speaker saat idle
#define AUDIO_POWER_TIMEOUT_MS 30000
```

Hemat ~10-20mA saat idle.

---

## Thermal Management

### Heat Sources

| Component | Heat | Notes |
|-----------|------|-------|
| ESP32-C3 | Low | Efficient RISC-V |
| MAX98357A | Medium | Class D amp, efficient |
| INMP441 | Negligible | MEMS mic |
| SSD1306 | Low | OLED, efficient |

### Thermal Limits

- Operating: -40°C to +85°C
- Recommended: 0°C to +70°C

### Heat Dissipation

- PCB ground plane sebagai heatsink
- MAX98357A: thermal pad ke PCB
- Tidak perlu heatsink aktif

---

## Battery Life Estimation

### Assumptions

- Battery: 1000mAh Li-ion
- Average current: 50mA

### Per Mode

| Mode | Avg Current | Battery Life |
|------|-------------|--------------|
| Xiaozhi idle | 30mA | ~33 jam |
| Xiaozhi active | 80mA | ~12 jam |
| Chronchi idle | 15mA | ~66 jam |
| Chronchi active | 20mA | ~50 jam |
| Xichi standby | 40mA | ~25 jam |
| Xichi speaking | 70mA | ~14 jam |

### Mixed Usage (Xichi)

```
Standby 90% + Speaking 10% = 0.9×40 + 0.1×70 = 43mA
→ ~23 jam battery life (1000mAh)
```

---

## Troubleshooting

| Masalah | Solusi |
|---------|--------|
| Baterai cepat habis | Cek power save level, kurangi active time |
| ADC tidak jalan | Cek GPIO1 wiring, cek voltage divider |
| Baterai salah baca | Kalibrasi ADC, cek resistor value |
| Device reboot saat TX | Cek power supply, kurangi TX power |
| Overheating | Cek MAX98357A thermal, kurangi volume |
| Sleep tidak bangun | Cek timer/interrupt config |
