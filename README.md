# Xiaozhi AI - ESP32-C3 Edition

Affordable solution for Xiaozhi AI using ESP32-C3.

## Features

- 🎤 **Voice Input** - INMP441 I2S microphone or WeAct ES8311 on-board mic
- 🔊 **Voice Output** - Hardware auto-detection: **MAX98357A** I2S amplifier OR **WeAct Studio ES8311 + NS4150B** PA amplifier
- 📺 **OLED Display** - SSD1306 128x64 display (I2C on GPIO 0 SDA / GPIO 10 SCL)
- 🔘 **Dual-Mode Button** - Xiaozhi AI or Chronchi/Chronos from one GPIO 3 UX
- 🔔 **Chronchi Smart Alerts** - Notification **"tit"** chime + 2x haptic vibration, plus loud **double-ring phone alarm** for "Temukan Perangkat Saya" (Find My Device)
- 🗺️ **Turn-by-Turn Navigation** - Real-time Google Maps/Waze navigation with dynamic 48x48 icons, direction maneuvers, street names, and ETA
- 📱 **Chronos & ESPBridge BLE** - Native ESP-NimBLE companion mode, mutually exclusive with Xiaozhi Wi-Fi/audio for maximum stability
- 📶 **WiFi** - Easy configuration via hotspot (192.168.4.1)

See the [dual-mode user guide](docs/dual-mode-user-guide.md), [Chronos port notes](docs/chronos-port.md), and [verification report](docs/dual-mode-test-report.md).

## Hardware Options

You can build with either discrete modules or the WeAct Studio all-in-one audio board:

| Component | Discrete Option | WeAct Studio Option |
|-----------|-----------------|---------------------|
| MCU | ESP32-C3 Super Mini (4MB) | ESP32-C3 Super Mini (4MB) |
| Microphone | INMP441 I2S MEMS | Built-in WeAct mic (ES8311 ADC) |
| Amplifier | MAX98357A I2S Class-D | Built-in NS4150B (ES8311 DAC) |
| Speaker | 3W 4Ω / 8Ω | 3W 4Ω / 8Ω |
| Display | SSD1306 OLED 128x64 I2C | SSD1306 OLED 128x64 I2C |
| Vibration | 3V Coin Motor (GPIO 20) | 3V Coin Motor (GPIO 20) |
| Button | Push button (GPIO 3) | Push button (GPIO 3) |

## Wiring Diagram

### Option A: Discrete Modules (INMP441 + MAX98357A)
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
```

### Option B: WeAct Studio Audio Board (ES8311 + NS4150B)
```
ESP32-C3          WeAct ES8311 Module
─────────         ───────────────────
GPIO 1  ────────► MCK (MCLK - Master Clock)
GPIO 5  ────────► BCK (BCLK - Bit Clock)
GPIO 6  ────────► WS (LRCK - Word Select)
GPIO 4  ────────► DO (ASDOUT - Mic Data)
GPIO 7  ────────► DI (DSDIN - Speaker Data)
GPIO 0  ────────► SDA (I2C shared with OLED)
GPIO 10 ────────► SCL (I2C shared with OLED)
GPIO 2  ────────► PA / CTRL (Power Amp Enable)
5V      ────────► 5V / VCC
GND     ────────► GND
```

### Peripherals (OLED, Button, Vibration)
```
ESP32-C3          Peripherals
─────────         ───────────
GPIO 0  ────────► SSD1306 OLED SDA
GPIO 10 ────────► SSD1306 OLED SCL
GPIO 3  ────────► Push Button (One side to GPIO 3, other side to GND)
GPIO 20 ────────► Vibration Motor (+) via NPN transistor / driver
3.3V    ────────► OLED VCC
GND     ────────► OLED GND & Button GND
```

### Pin Summary

| ESP32-C3 Pin | Function |
|--------------|----------|
| GPIO 0 | I2C SDA (SSD1306 OLED & ES8311 Codec) |
| GPIO 10 | I2C SCL (SSD1306 OLED & ES8311 Codec) |
| GPIO 1 | I2S MCLK (Master Clock for WeAct ES8311) |
| GPIO 2 | NS4150B PA Enable (WeAct) / Secondary Button (Discrete) |
| GPIO 3 | Primary Button: Short click chat/stop; Hold 2.5s mode menu |
| GPIO 4 | Mic data in (INMP441 SD / WeAct DO) |
| GPIO 5 | I2S BCLK (shared mic & speaker) |
| GPIO 6 | I2S WS/LRC (shared mic & speaker) |
| GPIO 7 | Speaker data out (MAX98357A DIN / WeAct DI) |
| GPIO 20 | Vibration Motor (Haptic alerts) |

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

### 4. Chronchi Mode (BLE Phone Companion via Chronos & ESPBridge)

ESP32-C3 is constrained to ~400KB SRAM. In Chronchi mode, Wi-Fi and WakeNet are completely shut down (**Mutual Exclusion**), and 100% of radio resources are dedicated to Bluetooth Low Energy (ESP-NimBLE). The audio subsystem is automatically configured to support both **MAX98357A** and **WeAct Studio ES8311**.

- **Android Companion App**: Pair with **Chronos** (recommended, by fbiego) or **ESPBridge** app (`Chronchi-XXYY`).
- **Secure Pairing**: Displays a 6-digit numeric comparison code on OLED; press GPIO 3 to confirm.
- **Notification Sound ("Tit") & Haptic Alert**:
  - Saat notifikasi pesan masuk (WhatsApp, Telegram, SMS, dll.), speaker membunyikan chime elektronik **"tit"** yang jernih (70 ms, 2000 Hz, anti-pop) bersamaan dengan **2x getaran haptik pendek** pada motor getar (GPIO 20).
- **Temukan Perangkat Saya (Find My Device) Alarm Berdering**:
  - Saat tombol *Temukan Perangkat Saya* ditekan di aplikasi Chronos (atau panggilan telepon masuk), speaker berdering nyaring dengan nada alarm telepon klasik ganda (**double-ring phone warble** 1568 Hz $\leftrightarrow$ 2093 Hz pada 100% volume) disertai getaran berdenyut secara sinkron.
  - Tekan tombol **GPIO 3** sekali pada jam, klik batal dari aplikasi Chronos, atau tunggu 30 detik (*safety timeout*) untuk menghentikan dering alarm seketika.
- **Dukungan Speaker Hardware Ganda**:
  - Mendeteksi otomatis modul **WeAct Studio ES8311 + NS4150B PA** via I2C (`0x18`/`0x19`) dengan MCLK GPIO 1 & PA Enable GPIO 2, atau amplifier I2S **MAX98357A** (BCLK GPIO 5, WS GPIO 6, DOUT GPIO 7).
- **6 OLED SSD1306 Screens**:
  1. **Home Screen**: Jam sinkron (WIB/UTC+7), indikator baterai HP, sinyal (4G/5G/WiFi), dan cuaca/suhu lokal.
  2. **Notification Screen**: Notifikasi langsung WhatsApp, Telegram, SMS, Gmail, Instagram (nama pengirim + isi pesan).
  3. **Financial & Order Screen**: Notifikasi transaksi e-wallet (GoPay, OVO, DANA) dan pelacakan pesanan e-commerce (Shopee, Tokopedia, Grab).
  4. **Turn-by-Turn Navigation (Google Maps & Waze)**:
     - Penerimaan icon navigasi dinamis 48x48 (paket `0xEE`/`0xEF`).
     - Render icon arah belokan tajam Google Maps-style 32x32 (lurus, belok kiri/kanan, U-turn, bundaran, dll.).
     - Tampilan jarak sisa belokan, nama jalan/tujuan, dan perkiraan waktu tiba (ETA).
  5. **Device Status**: Versi firmware dan status koneksi BLE.
  6. **BLE Wi-Fi Provisioning & BLE OTA**: Pengaturan Wi-Fi dan update firmware langsung secara nirkabel melalui BLE.

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
