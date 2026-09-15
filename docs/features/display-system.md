# Feature: Display System (SSD1306 OLED)

---

## Overview

ESP32-C3 menggunakan SSD1306 128x64 OLED display via I2C. Display digunakan untuk menampilkan status, notifikasi, mode menu, dan UI Chronchi.

---

## Hardware

### I2C Configuration

| Pin | Function | Connected To |
|-----|----------|--------------|
| GPIO 8 | SDA (Data) | SSD1306 SDA |
| GPIO 9 | SCL (Clock) | SSD1306 SCL |
| 3.3V | Power | SSD1306 VCC |
| GND | Ground | SSD1306 GND |

### Display Specs

- Resolution: 128 x 64 pixels
- Color: Monochrome (white on black)
- Interface: I2C (address 0x3C default)
- Driver: SSD1306

### I2C Settings

```c
// boards/esp32c3-inmp441/config.h
#define DISPLAY_SDA_PIN     GPIO_NUM_8
#define DISPLAY_SCL_PIN     GPIO_NUM_9
#define DISPLAY_WIDTH       128
#define DISPLAY_HEIGHT      64
#define DISPLAY_MIRROR_X    true   // Mirror horizontal
#define DISPLAY_MIRROR_Y    true   // Mirror vertical
```

### Optional Pull-up Resistors

Jika OLED tidak terdeteksi:
```
SDA ── 4.7kΩ ── 3.3V
SCL ── 4.7kΩ ── 3.3V
```

---

## Display Interface

### Base Class

```cpp
class Display {
public:
    virtual void SetStatus(const char* status);
    virtual void ShowNotification(const char* notification, int duration_ms = 3000);
    virtual void SetEmotion(const char* emotion);
    virtual void SetChatMessage(const char* role, const char* content);
    virtual void SetTheme(Theme* theme);
    virtual void UpdateStatusBar(bool update_all = false);
    virtual void SetPowerSaveMode(bool on);
    virtual void SetFaceState(FaceState state);
    
    // Mode menu
    virtual void ShowModeMenu(int selected_index);
    virtual void HideModeMenu();
    virtual void ShowModeSwitching(const char* mode_name);
    
    // Chronchi-specific
    virtual void SetChronchiScreen(const ChronchiScreen& screen);
};
```

### OLED Implementation

```cpp
class OledDisplay : public Display, public LvglDisplay {
    // LVGL-based rendering
    // 128x64 canvas
    // Status bar (top)
    // Main content area (middle)
    // Face/emoji area
};
```

---

## Display Layout

### Status Bar (Top Row)

```
┌─────────────────────────┐
│ ⚡85%  📶WiFi  🕐14:30  │
└─────────────────────────┘
```

| Element | Icon | Description |
|---------|------|-------------|
| Battery | ⚡XX% | Baterai ESP32 (ADC) |
| Network | 📶WiFi / 📶4G / 📶OFF | Status jaringan |
| Time | 🕐HH:MM | Jam (dari server/HP) |

### Xiaozhi Mode Display

**Idle:**
```
┌─────────────────────────┐
│ ⚡85%  📶WiFi  🕐14:30  │
│                         │
│        😊               │  Emoji face
│                         │
│ Ready                   │  Status text
└─────────────────────────┘
```

**Listening:**
```
┌─────────────────────────┐
│ ⚡85%  📶WiFi  🕐14:30  │
│                         │
│        🎤               │  Mic icon
│                         │
│ Mendengarkan...         │
└─────────────────────────┘
```

**Speaking:**
```
┌─────────────────────────┐
│ ⚡85%  📶WiFi  🕐14:30  │
│                         │
│        🔊               │  Speaker icon
│                         │
│ Response text here...   │  AI response
└─────────────────────────┘
```

### Chronchi Mode Display

**Home:**
```
┌─────────────────────────┐
│ ⚡85%  📶4G  🕐14:30    │  Status bar (HP data)
│                         │
│    😊                   │  Face
│                         │
│ 28°C  Jakarta           │  Cuaca + lokasi
│                         │
│ 📱HP:78% 🔋CHG          │  Baterai HP
└─────────────────────────┘
```

**Notification:**
```
┌─────────────────────────┐
│ WhatsApp         14:30  │  Source + time
│                         │
│ Budi: otw ya            │  Primary text
│ 5 menit lagi sampai     │  Secondary text
│                         │
└─────────────────────────┘
```

**Navigation:**
```
┌─────────────────────────┐
│ NAVIGATION       14:35  │
│                         │
│ ← Belok kiri            │  Direction
│ 200 m                   │  Distance
│ Jl. Sudirman            │  Road
│ 2.5 km                  │  Remaining
└─────────────────────────┘
```

### Xichi Mode Display

**Standby:**
```
┌─────────────────────────┐
│ ⚡85%  📶WiFi  🕐14:30  │
│                         │
│    XICHI                │  Mode name
│    STANDBY              │  State
│                         │
│ Menunggu notifikasi     │
└─────────────────────────┘
```

**Speaking:**
```
┌─────────────────────────┐
│ ⚡85%  📶WiFi  🕐14:30  │
│                         │
│    🔊                   │
│    MEMBACAKAN           │
│                         │
│ GoPay: Pembayaran..     │  Notif preview
└─────────────────────────┘
```

---

## Mode Menu

### 3-Option Menu (Xiaozhi / Chronchi / Xichi)

```
┌─────────────────────────┐
│                         │
│  > XIAOZHI              │  ← Selected
│    CHRONCHI             │
│    XICHI                │
│                         │
│  CLICK:NEXT  HOLD:OK    │
└─────────────────────────┘
```

### Navigation

| Action | Function |
|--------|----------|
| Short click GPIO3 | Cycle ke mode berikutnya |
| Hold 2 detik | Confirm & restart ke mode |
| Timeout 10 detik | Cancel, kembali ke mode sebelumnya |

### Implementation

```cpp
void OledDisplay::ShowModeMenu(int selected) {
    static const char* menus[] = {
        "> XIAOZHI\n  CHRONCHI\n  XICHI",
        "  XIAOZHI\n> CHRONCHI\n  XICHI",
        "  XIAOZHI\n  CHRONCHI\n> XICHI",
    };
    lv_label_set_text(mode_selection_label_, menus[selected]);
    lv_label_set_text(mode_help_label_, "CLICK:NEXT  HOLD:OK");
    lv_obj_remove_flag(mode_overlay_, LV_OBJ_FLAG_HIDDEN);
}
```

---

## LVGL Integration

### Display Lock

```cpp
// Semua operasi display harus di-lock
DisplayLockGuard lock(this);
lv_label_set_text(label, "text");
```

### Font

- Default: puhui_14_1 (Chinese + Latin)
- Icon: font_awesome_14_1

### Canvas

```cpp
lv_obj_t* canvas = lv_canvas_create(lv_screen_active());
lv_canvas_set_buffer(canvas, buf, 128, 64, LV_COLOR_FORMAT_L8);
```

---

## Notifications

### Show Notification

```cpp
// Tampilkan notifikasi selama 3 detik
display_->ShowNotification("WiFi Connected", 3000);

// Tampilkan terus (sampai di-clear)
display_->ShowNotification("Menunggu...", 0);
```

### Auto-hide

Notifikasi otomatis hilang setelah `duration_ms`. Jika `duration_ms = 0`, tampilkan terus.

---

## Power Save

```cpp
// Matikan display (hemat power)
display_->SetPowerSaveMode(true);

// Nyalakan display
display_->SetPowerSaveMode(false);
```

---

## Chronchi Screen Types

```cpp
enum class ChronchiScreenType {
    Home,         // Default home screen
    Message,      // Chat message
    Email,        // Email notification
    Professional, // Work notification
    Payment,      // Payment notification
    Order,        // Order status
    System,       // System notification
    Navigation,   // Navigation instruction
};
```

### Priority (tinggi → rendah)

| Priority | Type | Timeout |
|----------|------|---------|
| 1 | Navigation | Sampai `active=false` |
| 2 | Payment | 30 detik |
| 3 | Order | 30 detik |
| 4 | Message | 20 detik |
| 5 | Professional | 20 detik |
| 6 | Email | 20 detik |
| 7 | System | 10 detik |
| 8 | Home | Selalu |

Higher priority screen menggantikan lower priority. Setelah timeout, kembali ke home atau screen sebelumnya.

---

## Troubleshooting

| Masalah | Solusi |
|---------|--------|
| OLED tidak terdeteksi | Cek wiring SDA→GPIO8, SCL→GPIO9 |
| OLED tidak terdeteksi | Tambah pull-up resistor 4.7kΩ |
| Display terbalik | Cek DISPLAY_MIRROR_X/Y config |
| Teks terpotong | Cek font size, kurangi teks |
| Display freeze | Cek LVGL lock, cek heap |
| Garbage di layar | Reset I2C, re-init display |
