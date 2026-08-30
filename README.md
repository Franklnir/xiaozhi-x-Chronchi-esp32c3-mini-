# Xiaozhi AI - ESP32-C3 Edition

Affordable solution for Xiaozhi AI using ESP32-C3.

## Features

- 🎤 **Voice Input** - INMP441 I2S microphone
- 🔊 **Voice Output** - MAX98357A I2S amplifier
- 📺 **Optional OLED** - SSD1306 128x64 display
- 🔘 **Dual-mode button** - Xiaozhi AI or Chronos/Mochi from one GPIO3 UX
- **Chronos/Mochi BLE** - Native ESP-NimBLE mode, mutually exclusive with Xiaozhi Wi-Fi/audio
- 📶 **WiFi** - Easy configuration via hotspot

See the [dual-mode user guide](docs/dual-mode-user-guide.md), [Chronos port notes](docs/chronos-port.md), and [verification report](docs/dual-mode-test-report.md).

## Hardware Required

| Component | Description | Cost (IDR) | Cost (USD) |
|-----------|-------------|------------|------------|
| ESP32-C3 | Dev board (4MB flash) | Rp 35,000 | $2.20 |
| INMP441 | I2S MEMS microphone | Rp 25,000 | $1.60 |
| MAX98357A | I2S amplifier | Rp 20,000 | $1.25 |
| Speaker | 3W 4Ω | Rp 10,000 | $0.65 |
| Push Button | Momentary switch | Rp 1,000 | $0.10 |
| SSD1306 OLED | 128x64 I2C (optional) | Rp 25,000 | $1.60 |
| **Total** | **(without OLED)** | **Rp 91,000** | **$5.80** |
| **Total** | **(with OLED)** | **Rp 116,000** | **$7.40** |

## Wiring Diagram

```
ESP32-C3          INMP441 (Mic)      MAX98357A (Speaker)
─────────         ─────────────      ───────────────────
GPIO 5  ────────► SCK                BCLK
GPIO 6  ────────► WS                 LRC  
GPIO 4  ────────► SD                 
GPIO 7  ──────────────────────────►  DIN
3.3V    ────────► VDD                
5V      ──────────────────────────►  VIN
GND     ────────► GND                GND
        ────────► L/R → GND

ESP32-C3          Push Button
─────────         ───────────
GPIO 3  ────────► One side (mode normal)
GPIO 2  ────────► 1x klik untuk aktifkan mode ngobrol otomatis, jika lebih dari 12 detik gk ada respon xiaozhi masuk ke mode low power, cara bangunin nya tinggal katakan "hi jason"
GND     ────────► Other side

ESP32-C3          SSD1306 OLED (Optional)
─────────         ──────────────────────
GPIO 8  ────────► SDA
GPIO 9  ────────► SCL (or SCK)
3.3V    ────────► VCC
GND     ────────► GND
```

### Pin Summary

| ESP32-C3 Pin | Function |
|--------------|----------|
| GPIO 3 | Short-click chat/page; hold 5 seconds for mode menu |
| GPIO 2 | Reset SSID button (optional, hold 3 seconds) |
| GPIO 4 | Mic data (INMP441 SD) |
| GPIO 5 | I2S BCLK (shared) |
| GPIO 6 | I2S WS/LRC (shared) |
| GPIO 7 | Speaker data (MAX98357A DIN) |
| GPIO 8 | OLED SDA (optional) |
| GPIO 9 | OLED SCL (optional) |

## Installation

### Option 1: Easy Flash (Recommended)

1. Download the latest firmware from [Releases](https://github.com/mniroy/xiaozhi-ai-esp32-c3/releases)
2. Go to [Web Flasher](https://mniroy.github.io/Escher_ESP_Flasher/)
3. Connect your ESP32-C3 via USB
4. Click "Connect" and select your device
5. Upload the `.bin` file and flash at address `0x0`

### Option 2: Build from Source

**Prerequisites:** [ESP-IDF v5.5+](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/)

```bash
# Clone the repository
git clone https://github.com/mniroy/xiaozhi-ai-esp32-c3.git
cd xiaozhi-ai-esp32-c3

# Set target to ESP32-C3
idf.py set-target esp32c3

# Build and flash
idf.py build flash monitor
```

## Usage

### First Boot - WiFi Setup

1. Power on the device
2. **Click the button** during startup (within 5 seconds)
3. Connect to the WiFi hotspot: `Xiaozhi-XXXX`
4. Open http://192.168.4.1 in browser
5. Enter your WiFi credentials
6. Device will restart and connect

### Talking to AI

1. **Hold the button** and speak
2. **Release the button** when done
3. Wait for AI response

### Reset Saved WiFi (SSID)

1. Connect an optional button from **GPIO 2** to **GND**
2. **Hold the button for 3 seconds**
3. Device will clear saved WiFi credentials and reboot
4. After reboot, device enters WiFi setup mode again

## Troubleshooting

### No Sound Output
- Check MAX98357A is powered by **5V** (not 3.3V)
- Verify GPIO 7 is connected to DIN
- Add a 100µF capacitor between VIN and GND

### Mic Not Working
- Ensure INMP441 L/R pin is connected to **GND** (left channel)
- Check GPIO 4 is connected to SD

### OLED Not Detected
- Verify I2C wiring (SDA→GPIO8, SCL→GPIO9)
- Try adding 4.7kΩ pull-up resistors on SDA and SCL to 3.3V
- Device works fine without OLED

## License

Based on [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) project.

## Credits

- Original Xiaozhi project by [78](https://github.com/78)
- ESP32-C3 port by [mniroy](https://github.com/mniroy)
