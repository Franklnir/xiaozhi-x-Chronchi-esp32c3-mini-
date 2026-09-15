# ESP32-C3 Xiaozhi AI — Master Documentation Index

**Version:** 2.0  
**Last Updated:** 2026-09-07  
**Hardware:** ESP32-C3 Mini + INMP441 + MAX98357A + SSD1306

---

## Modes

| Mode | File | Description |
|------|------|-------------|
| **Xiaozhi** | [modes/xiaozhi-mode.md](modes/xiaozhi-mode.md) | Voice AI assistant — bicara dengan AI lewat speaker |
| **Chronchi** | [modes/chronchi-mode.md](modes/chronchi-mode.md) | Phone companion — tampilkan data HP di OLED via BLE |
|| **Xichi** | [modes/xichi-mode.md](modes/xichi-mode.md) | Notification reader — notifikasi HP dibacakan via Firebase + Wi-Fi |

## Features

| Feature | File | Description |
|---------|------|-------------|
| **Audio System** | [features/audio-system.md](features/audio-system.md) | I2S, Opus codec, speaker, microphone, volume control |
| **BLE Communication** | [features/ble-communication.md](features/ble-communication.md) | NimBLE, ESPBridge protocol, GATT service, pairing |
| **Wi-Fi & Networking** | [features/wifi-networking.md](features/wifi-networking.md) | Wi-Fi STA, coexistence, WebSocket/MQTT, server connection |
| **MCP Protocol** | [features/mcp-protocol.md](features/mcp-protocol.md) | Model Context Protocol — tool calls, device control |
| **Display System** | [features/display-system.md](features/display-system.md) | SSD1306 OLED, LVGL, mode menu, notifications, Chronchi UI |
| **Button Controls** | [features/button-controls.md](features/button-controls.md) | GPIO3/GPIO2, push-to-talk, mode selection, wake word trigger |
| **Power Management** | [features/power-management.md](features/power-management.md) | Battery monitor, sleep, power save levels |

## Quick Reference

### Hardware Pinout

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

ESP32-C3          SSD1306 OLED
─────────         ────────────
GPIO 8  ────────► SDA
GPIO 9  ────────► SCL
3.3V    ────────► VCC
GND     ────────► GND

ESP32-C3          Buttons
─────────         ───────
GPIO 3  ────────► BOOT button (mode select / PTT)
GPIO 2  ────────► RESET button (optional)
```

### Mode Selection

```
Power on → Hold BOOT 2.5 detik → Mode menu muncul di OLED
  Short click: cycle (Xiaozhi → Chronchi → Xichi → Xiaozhi)
  Hold 2 detik: confirm & restart ke mode yang dipilih
```

### Partition Table (4MB Flash)

```
Name     Type    Offset     Size
nvs      data    0x9000     16 KB
otadata  data    0xD000     8 KB
phy_init data    0xF000     4 KB
rescue   app     0x10000    576 KB   (BLE recovery updater)
main     app     0xA0000    2.6 MB   (ota_0 — full product app)
assets   data    0x340000   768 KB   (SPIFFS — fonts, OGG, emoji)
```

### RAM Budget Summary

| Component | Xiaozhi | Chronchi | Xichi |
|-----------|---------|----------|-------|
| Wi-Fi stack | 80-100 KB | 0 KB | 80-100 KB |
| NimBLE stack | 0 KB | 40-50 KB | 0 KB |
| Audio service | 60-80 KB | 0 KB | 30-40 KB |
| Wake word engine | 15-20 KB | 0 KB | 0 KB |
| Application | 40-50 KB | 30-40 KB | 30-40 KB |
| **Total used** | **195-250 KB** | **70-90 KB** | **140-180 KB** |
| **Free heap** | **80-135 KB** | **240-260 KB** | **150-190 KB** |

### Build Commands

```bash
# Set target
idf.py set-target esp32c3

# Build
idf.py build

# Flash
idf.py -p COM_PORT flash

# Monitor
idf.py -p COM_PORT monitor

# Build + Flash + Monitor
idf.py -p COM_PORT build flash monitor
```
