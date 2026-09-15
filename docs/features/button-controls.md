# Feature: Button Controls

---

## Overview

ESP32-C3 menggunakan 2 tombol: GPIO3 (BOOT button) sebagai tombol utama untuk push-to-talk dan mode selection, serta GPIO2 sebagai tombol reset WiFi (opsional).

---

## Hardware

### GPIO3 (BOOT Button)

```
GPIO 3 ──── Push Button ──── GND
```

- **Internal pull-up:** Aktif (default HIGH)
- **Active:** LOW saat ditekan
- **Fungsi:**
  - Short click (< 700ms): Push-to-talk / Next mode
  - Hold 2.5 detik: Masuk mode menu
  - Hold 2 detik (di menu): Confirm mode selection
  - Hold 5 detik: Mode menu (extended)

### GPIO2 (RESET Button) — Optional

```
GPIO 2 ──── Push Button ──── GND
```

- **Internal pull-up:** Aktif (default HIGH)
- **Active:** LOW saat ditekan
- **Fungsi:**
  - Hold 3 detik: Reset WiFi credentials
  - Click: Hands-free toggle (Xiaozhi mode)

---

## Button Class

```cpp
class Button {
public:
    Button(gpio_num_t gpio);
    
    void OnPressDown(std::function<void()> callback);
    void OnPressUp(std::function<void()> callback);
    void OnLongPress(std::function<void()> callback, int duration_ms);
    
private:
    gpio_num_t gpio_;
    std::function<void()> on_press_down_;
    std::function<void()> on_press_up_;
    std::function<void()> on_long_press_;
};
```

### Implementation

```cpp
Button::Button(gpio_num_t gpio) : gpio_(gpio) {
    gpio_config_t config = {};
    config.pin_bit_mask = (1ULL << gpio);
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_ANYEDGE;
    gpio_config(&config);
    
    // Install GPIO ISR
    gpio_install_isr_service(0);
    gpio_isr_handler_add(gpio, GpioIsrHandler, this);
}
```

---

## ModeSelector Class

### Timing Constants

```cpp
static constexpr int kShortClickMs = 700;    // < 700ms = short click
static constexpr int kEnterMenuMs = 2500;    // > 2.5s = enter menu
static constexpr int kConfirmMs = 2000;      // > 2s = confirm in menu
static constexpr int kMenuTimeoutMs = 10000; // 10s timeout = cancel
```

### State Machine

```
┌─────────┐
│  IDLE   │ Button released
└────┬────┘
     │ Press down
     ▼
┌─────────┐
│ PRESSED │ Timer mulai
└────┬────┘
     │ < 700ms release
     │ (SHORT CLICK)
     ▼
┌─────────┐
│ ACTION  │ Execute short click callback
└─────────┘

     │ > 2.5s still pressed
     ▼
┌──────────┐
│ IN MENU  │ Mode selection menu aktif
└────┬─────┘
     │ Short click (< 700ms)
     │ (CYCLE MODE)
     ▼
┌──────────┐
│ NEXT MODE│ Cycle: Xiaozhi → Chronchi → Xichi → Xiaozhi
└──────────┘

     │ Hold > 2s in menu
     ▼
┌──────────┐
│ CONFIRM  │ Save mode & restart
└──────────┘

     │ Timeout 10s
     ▼
┌──────────┐
│ CANCEL   │ Hide menu, kembali ke mode sebelumnya
└──────────┘
```

### Implementation

```cpp
void ModeSelector::OnPressUp() {
    std::lock_guard<std::mutex> lock(mutex_);
    const int64_t held_us = esp_timer_get_time() - press_started_us_;
    
    if (menu_active_) {
        if (must_release_) {
            // First press after entering menu - ignore
            must_release_ = false;
            menu_deadline_us_ = esp_timer_get_time() + MsToUs(kMenuTimeoutMs);
        } else if (!confirm_fired_ && held_us < MsToUs(kShortClickMs)) {
            // Short click in menu → cycle mode
            int current = static_cast<int>(selected_mode_);
            selected_mode_ = static_cast<BootMode>((current + 1) % 3);
            menu_deadline_us_ = esp_timer_get_time() + MsToUs(kMenuTimeoutMs);
        }
    } else if (held_us < MsToUs(kShortClickMs)) {
        // Short click → push-to-talk
        if (short_click_) short_click_();
    }
}

void ModeSelector::Tick() {
    std::lock_guard<std::mutex> lock(mutex_);
    const int64_t now = esp_timer_get_time();
    
    if (!menu_active_ && pressed_ && 
        now - press_started_us_ >= MsToUs(kEnterMenuMs)) {
        // Enter menu
        menu_active_ = true;
        must_release_ = true;
        selected_mode_ = current_mode_;
        menu_deadline_us_ = now + MsToUs(kMenuTimeoutMs);
        // Show menu on display
    }
    
    if (menu_active_ && !must_release_ && pressed_ && !confirm_fired_ &&
        now - press_started_us_ >= MsToUs(kConfirmMs)) {
        // Confirm selection
        confirm_fired_ = true;
        menu_active_ = false;
        Confirm(selected_mode_);
    }
    
    if (menu_active_ && now >= menu_deadline_us_) {
        // Timeout → cancel
        menu_active_ = false;
        must_release_ = false;
    }
}
```

---

## Button Actions per Mode

### Xiaozhi Mode

| Action | Function |
|--------|----------|
| Short click | Push-to-talk (mulai recording) |
| Release | Stop recording, kirim ke server |
| Hold 2.5s | Enter mode menu |
| Wake word detected | Auto-start listening |

### Chronchi Mode

| Action | Function |
|--------|----------|
| Short click | Confirm BLE pairing (jika pending) |
| Hold 2.5s | Enter mode menu |

### Xichi Mode

| Action | Function |
|--------|----------|
| Short click | Push-to-talk (temporary chat mode) |
| Release | Stop recording, kirim ke server |
| Hold 2.5s | Enter mode menu |

---

## GPIO2 (RESET Button) Functions

### WiFi Reset

```
Hold 3 detik → Clear WiFi credentials → Reboot
```

**Voice confirmation:**
- "reset wifi" → ESP32 tanya "Konfirmasi reset WiFi?"
- "ya" / "iya" dalam 15 detik → Reset & reboot
- "batal" → Cancel

### Hands-free Toggle (Xiaozhi only)

```
Click → Toggle hands-free mode
  - ON: Auto-listen setelah boot
  - OFF: Push-to-talk only
```

---

## Debouncing

### Hardware Debounce

```cpp
// GPIO internal pull-up + capacitor
// Tambah 100nF capacitor dari GPIO ke GND untuk debounce hardware
```

### Software Debounce

```cpp
// ISR handler dengan debounce timer
static void GpioIsrHandler(void* arg) {
    Button* button = static_cast<Button*>(arg);
    int64_t now = esp_timer_get_time();
    
    if (now - button->last_isr_time_ < 50000) { // 50ms debounce
        return;
    }
    button->last_isr_time_ = now;
    
    int level = gpio_get_level(button->gpio_);
    if (level == 0) {
        // Press down
        if (button->on_press_down_) button->on_press_down_();
    } else {
        // Press up
        if (button->on_press_up_) button->on_press_up_();
    }
}
```

---

## Wake Word Detection

### ESP-SR Wake Word Engine

```cpp
// Wake word models disimpan di assets partition
// Model: wn9_hilexin (custom wake word "Hi Lexin")

// Enable/disable
audio_service_.EnableWakeWordDetection(true);
```

### Behavior

- Mendengarkan terus di background
- Saat terdeteksi → callback ke Application
- Application mulai recording
- Ada debounce (2 detik) dan cooldown (2 detik setelah TTS)

### Xichi Mode

Wake word DIMATIKAN total:
```cpp
// Di XichiMode::Initialize()
audio_service_.EnableWakeWordDetection(false);
```

---

## Troubleshooting

| Masalah | Solusi |
|---------|--------|
| Tombol tidak responsif | Cek wiring GPIO→GND, cek pull-up |
| Double press | Tambah debounce capacitor/software |
| Mode menu tidak muncul | Cek hold duration ≥ 2.5 detik |
| Wake word tidak deteksi | Cek model file di assets, cek mic |
| Push-to-talk tidak jalan | Cek GPIO3 wiring, cek audio service |
