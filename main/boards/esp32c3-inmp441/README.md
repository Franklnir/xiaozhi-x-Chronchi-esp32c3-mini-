# ESP32-C3 with INMP441 + MAX98357A + OLED

A XiaoZhi AI board for ESP32-C3 using:
- **Microphone**: INMP441 (I2S)
- **Speaker**: MAX98357A (I2S)
- **Display**: SSD1306 OLED 128x64 (I2C)
- **Buttons**: GPIO3 chat/dual-mode selector, GPIO2 legacy hands-free toggle/reset

## Wiring

| ESP32-C3 Pin | Device | Function |
|--------------|--------|----------|
| GPIO 5 | INMP441 + MAX98357A | BCLK (shared) |
| GPIO 6 | INMP441 + MAX98357A | WS/LRC (shared) |
| GPIO 4 | INMP441 | SD (Mic Data) |
| GPIO 7 | MAX98357A | DIN (Speaker Data) |
| GPIO 3 | Button | Short-click chat/page; hold 2.5s for dual-mode menu |
| GPIO 2 | Button (optional) | Hands-free toggle (click), Reset SSID (hold 5s) |
| GPIO 8 | SSD1306 OLED | SDA |
| GPIO 9 | SSD1306 OLED | SCL |
| GPIO 1 | Li-ion battery divider | ADC1_CH1 battery sense |
| 5V | MAX98357A | VIN |
| 3V3 | INMP441, SSD1306 | VDD |
| GND | All | GND |

### Battery percentage wiring

Use a two-resistor voltage divider for a single-cell Li-ion/LiPo battery:

```text
BAT+ ---- 100k ----+---- GPIO1 (ADC)
                   |
                  100k
                   |
GND ---------------+---- GND

GPIO1 ---- 100 nF ---- GND
```

- Two 100k resistors are required; never connect BAT+ directly to GPIO1.
- Place the optional but recommended 100 nF ceramic capacitor close to GPIO1.
- The firmware samples the divided voltage every 5 seconds, filters it, maps it
  to a Li-ion state-of-charge curve, and shows the local device percentage at
  the OLED's upper-right corner.
- Charging state is estimated from voltage trend because this board definition
  does not reserve a dedicated charger-status pin.

## Usage

1. **First boot**: Xiaozhi enters its provisioning flow when no credential is stored; a short GPIO3 click during startup keeps the legacy Wi-Fi config shortcut
2. **GPIO3 short click**: Toggle manual chat in Xiaozhi, or advance the page in Chronos
3. **GPIO3 hold 2.5s**: Open the Xiaozhi/Chronos selector; release, click to choose, then hold 2s to confirm
4. **Hands-free mode (default ON)**: Just speak without pressing; after you stop talking and silence is detected, XiaoZhi responds
5. **Auto low power in hands-free**: If no voice is detected for 30 seconds, device closes audio channel and returns to low-power standby
6. **Wake by voice command**: Choose Hi Jason, Hi Lexin, Hi ESP, or Ni Hao Xiaozhi in portal **Advanced → Wake Word**
7. **GPIO2 toggle**: Click GPIO2 to switch hands-free ON/OFF
   OFF means fully disabled: no auto listen and no wake-by-voice.
8. **Reset WiFi credentials**: Hold GPIO2 for 5 seconds to clear saved SSID/password and reboot

Full instructions: [dual-mode user guide](../../../docs/dual-mode-user-guide.md). Protocol notes: [Chronos port](../../../docs/chronos-port.md).

The normal development variant uses `partitions/v2/4m-development.csv`, so a
regular `idf.py flash` writes the complete app to a correctly sized factory
partition. The production variant keeps using `partitions/v2/4m.csv` and must
be packaged with its separate permanent recovery image at the documented fixed
offsets; do not use the normal one-image flash command for a production unit.

## Local Song Command

- Voice command keywords: `nyanyi`, `putar lagu`, `sing`, `play song`
- Stop command while song is playing: `stop`, `berhenti`, `hentikan`, `pause`
- Song selection:
  - default: `song1`
  - contains `dua` / `2` / `two`: `song2`
  - contains `tiga` / `3` / `three`: `song3`
- GPIO3 short-click chat action can also interrupt/stop local song playback.
- File lookup order per song:
  - `/spiffs/songX.ogg`
  - `/spiffs/songs/songX.ogg`
  - `songX.ogg` (assets partition)
  - `songs/songX.ogg` (assets partition)
  - `music/songX.ogg` (assets partition)

## Voice Standby Command

- While device is in listening mode, say one of:
  - `stop`
  - `udahan dulu`
  - `byee` / `bye`
  - `sampai jumpa`
  - `sampai nanti`
- Device will leave listening mode and return to standby/low-power flow.

## Voice WiFi Reset (with confirmation)

- Step 1 (request reset): say `reset wifi` (or `ganti wifi` / `hapus wifi` / `reset ssid`)
- Step 2 (confirm within 15s): say `ya` (or `konfirmasi reset wifi`)
- Cancel pending reset: say `batal reset wifi`
- On success, device clears saved WiFi credentials and reboots.
