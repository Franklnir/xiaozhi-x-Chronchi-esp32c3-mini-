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

## Usage & Operational Guide

### 1. First Boot & Web Config Portal (192.168.4.1)

1. Power on the device.
2. If Wi-Fi is not yet configured (or after reset), the device starts a setup AP hotspot: `Xiaozhi-XXXX`.
3. Connect your smartphone/PC to the hotspot and navigate to **`http://192.168.4.1`**.
4. **Portal Configuration Options**:
   - **Wi-Fi Settings**: Select SSID and enter password (supports multi-SSID up to 10 networks).
   - **Wake Word Dropdown**:
     - `Hi Jason` (`hijason`) - **Default**: Highly optimized for clear detection and low false wake-up.
     - `Hi Lexin` (`hilexin`): Standard Espressif ESP-SR model.
     - `Hi ESP` (`hiesp`): Short activation keyword.
     - `Ni Hao Xiaozhi` (`nihaoxiaozhi`): Original Mandarin model.
   - **OLED Display Mode**: Choose between `Classic` (clean text) or `Animated` (expressive faces).
   - **Wi-Fi TX Power**: Select transmission power from 2 dBm to 20 dBm for optimal battery life vs. signal range.
5. Save settings; the device automatically saves to NVS and reboots to connect.

### 2. Physical Button Controls (Dual-Button Setup)

- **GPIO 3 (Primary Multi-Function Button)**:
  - **Short Click**: Talk to AI (Push-to-Talk) / Interrupt speaking AI / **Instant Stop (0ms)** for YouTube music playback.
  - **Double Click (Fast 2x)**: **Instant Mode Switch** between **Xiaozhi AI** ↔ **Chronchi BLE** mode without entering menus.
  - **Hold for 2.5s**: Open visual **`SELECT MODE`** menu on OLED. Release button, short-click to select mode (Xiaozhi / Chronchi), then hold for 2s to confirm and reboot.
  - **Triple Click (in Chronchi mode)**: Clear saved Wi-Fi and reboot directly into Web Config portal.
- **GPIO 2 (Secondary Button / Hands-Free & Safety Reset)**:
  - **Short Click**: Toggle **Hands-Free Mode (ON / OFF)**. When active, AI continuously listens automatically. If idle for >12s, enters low-power standby; wake up by saying *"Hi Jason"*.
  - **Hold for 5s**: **Hardware Hard Reset SSID** — immediately wipes Wi-Fi NVS credentials in emergency.

### 3. Built-in Voice Commands

- **Wi-Fi Reset with Two-Step Confirmation**:
  - Say: *"reset wifi"*, *"hapus wifi"*, *"ganti wifi"*, or *"clear wifi"*.
  - Device plays warning chime and displays: `Say: ya / konfirmasi reset wifi (15s)`.
  - Confirm by saying *"ya"*, *"iya"*, or *"konfirmasi reset wifi"*; device clears NVS and reboots to `192.168.4.1`.
  - Cancel anytime by saying *"batal reset wifi"* or waiting 15 seconds.
- **Voice Mode Switch**:
  - Say: *"pindah mode"*, *"ganti ke mode chronchi"*, or *"masuk mode chronchi"*.
  - Smart negation filter ignores commands containing *"jangan"*, *"tidak"*, or *"batal"*.
  - Reboots directly into the target mode.
- **Offline Local Music**:
  - Play: *"putar lagu"*, *"mainkan musik"*.
  - Stop: *"stop lagu"*, *"berhenti musik"*.
- **Standby & Conversation Exit**:
  - Say: *"stop"*, *"udahan dulu"*, *"bye"*, or *"sampai jumpa"*.
- **YouTube Adaptive Streaming**:
  - Say: *"putar lagu [title] di youtube"*.
  - Stop: Press **GPIO 3** once for **Instant Stop (0ms)**.

### 4. Chronchi Mode (BLE Phone Companion via ESPBridge)

ESP32-C3 is constrained to ~400KB SRAM. In Chronchi mode, Wi-Fi, audio, and WakeNet are completely shut down (**Mutual Exclusion**), and 100% of radio resources are dedicated to Bluetooth Low Energy (ESP-NimBLE).

- **Android Companion App**: Pair with **Chronos** or **ESPBridge** app (`Chronchi-XXYY`).
- **Secure Pairing**: Displays a 6-digit numeric comparison code on OLED; press GPIO 3 to confirm.
- **6 OLED SSD1306 Screens**:
  1. **Home Screen**: Synchronized clock (WIB/UTC+7), phone battery bar, signal level (4G/5G/WiFi), and local weather/temperature.
  2. **Notification Screen**: Live alerts from WhatsApp, Telegram, SMS, Gmail, Slack (sender name + message preview, active for 7s).
  3. **Financial & Order Screen**: E-wallet transactions (GoPay, OVO, DANA) and e-commerce parcel tracking (Shopee, Tokopedia, Grab).
  4. **Turn-by-Turn Navigation**: Real-time Google Maps & Waze directions (maneuver arrows, distance to turn, street names, ETA).
  5. **Device Status**: Firmware version and local battery monitor.
  6. **BLE Wi-Fi Provisioning & BLE OTA**: Wireless firmware updates directly over Bluetooth Low Energy.

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
