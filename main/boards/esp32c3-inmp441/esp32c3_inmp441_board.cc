/**
 * ESP32-C3 with INMP441 Microphone, MAX98357A Speaker, and SSD1306 OLED
 * 
 * Wiring:
 * GPIO 5 - BCLK (shared by mic and speaker)
 * GPIO 6 - WS/LRC (shared by mic and speaker)
 * GPIO 4 - INMP441 SD (Mic Data In)
 * GPIO 7 - MAX98357A DIN (Speaker Data Out)
 * GPIO 3 - Short-click chat + dual-mode selector button
 * GPIO 2 - Hands-free mode toggle + Reset SSID button (optional)
 * GPIO 8 - OLED SDA
 * GPIO 9 - OLED SCL
 */

#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/display.h"
#include "application.h"
#include "adc_battery_monitor.h"
#include "button.h"
#include "config.h"
#include "shared_oled.h"
#include "mode/mode_selector.h"
#include "mode/mode_store.h"
#include "settings.h"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <nvs.h>
#include <cstdio>
#include <memory>
#include <string>

#define TAG "Esp32c3Inmp441Board"

class Esp32c3Inmp441Board : public WifiBoard {
private:
    Button boot_button_;
    Button reset_ssid_button_;
    std::unique_ptr<ModeSelector> mode_selector_;
    esp_timer_handle_t hands_free_timer_ = nullptr;
    int64_t last_hands_free_trigger_us_ = 0;
    int64_t last_voice_activity_us_ = 0;
    bool hands_free_enabled_ = true;
    bool wait_for_wake_word_ = false;
    bool boot_button_pressed_ = false;
    DeviceState last_observed_state_ = kDeviceStateUnknown;

    void ShowWakeWordStandbyHint() {
        Settings audio_settings("audio", false);
        const std::string selected = audio_settings.GetString("wake_word", "hijason");
        const char* label = "Hi Jason";
        if (selected == "hilexin") label = "Hi Lexin";
        else if (selected == "hiesp") label = "Hi ESP";
        else if (selected == "nihaoxiaozhi") label = "Ni Hao Xiaozhi";
        char hint[64];
        std::snprintf(hint, sizeof(hint), "Standby, say: %s", label);
        GetDisplay()->ShowNotification(hint);
    }

    void ToggleHandsFreeMode() {
        hands_free_enabled_ = !hands_free_enabled_;
        auto& app = Application::GetInstance();
        if (hands_free_enabled_) {
            ESP_LOGI(TAG, "Hands-free mode enabled");
            GetDisplay()->ShowNotification("Hands-free ON");
            last_hands_free_trigger_us_ = 0;
            last_voice_activity_us_ = esp_timer_get_time();
            wait_for_wake_word_ = false;
            app.GetAudioService().EnableWakeWordDetection(true);
            // Enter listening immediately when enabled, then idle timeout will close it.
            if (app.GetDeviceState() == kDeviceStateIdle) {
                app.ToggleChatState();
            }
        } else {
            ESP_LOGI(TAG, "Hands-free mode disabled");
            GetDisplay()->ShowNotification("Hands-free OFF");
            wait_for_wake_word_ = false;
            // Keep wake-word detection enabled so standby can be woken by voice.
            app.GetAudioService().EnableWakeWordDetection(true);
            // If currently listening, close channel and return to idle.
            // If speaking, it may return to listening briefly, then timer branch will close it.
            if (app.GetDeviceState() == kDeviceStateListening) {
                app.ToggleChatState();
            }
        }
    }

    bool ClearStoredWifiCredentials() {
        nvs_handle_t nvs_handle = 0;
        esp_err_t ret = nvs_open("wifi", NVS_READWRITE, &nvs_handle);
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "WiFi namespace not found, nothing to clear");
            return true;
        }
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open WiFi namespace: %s", esp_err_to_name(ret));
            return false;
        }

        ret = nvs_erase_all(nvs_handle);
        if (ret == ESP_OK) {
            ret = nvs_commit(nvs_handle);
        }
        nvs_close(nvs_handle);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to clear WiFi credentials: %s", esp_err_to_name(ret));
            return false;
        }

        ESP_LOGI(TAG, "Stored WiFi credentials cleared");
        return true;
    }

    void InitializeButtons() {
        boot_button_.OnPressDown([this]() {
            boot_button_pressed_ = true;
            mode_selector_->OnPressDown();
        });
        boot_button_.OnPressUp([this]() {
            boot_button_pressed_ = false;
            mode_selector_->OnPressUp();
        });

        // GPIO2 single click toggles hands-free mode.
        reset_ssid_button_.OnClick([this]() {
            ToggleHandsFreeMode();
        });

        // Hold reset button to clear saved SSID/password and reboot.
        reset_ssid_button_.OnLongPress([this]() {
            ESP_LOGW(TAG, "Reset SSID button long pressed");

            if (!ClearStoredWifiCredentials()) {
                GetDisplay()->ShowNotification("Failed to reset SSID");
                return;
            }

            GetDisplay()->ShowNotification("SSID reset, rebooting...");
            esp_restart();
        });
    }

    void TryStartHandsFreeListening() {
#if HANDS_FREE_AUTO_LISTEN
        auto& app = Application::GetInstance();
        if (!hands_free_enabled_ || app.IsStandbyActive()) {
            // OFF means truly OFF:
            // - keep wake-word enabled for standby wake
            // - if channel is still listening, close it
            if (app.GetDeviceState() == kDeviceStateIdle) {
                app.GetAudioService().EnableWakeWordDetection(true);
            } else if (app.GetDeviceState() == kDeviceStateListening && !boot_button_pressed_) {
                app.ToggleChatState();
            }
            return;
        }

        auto state = app.GetDeviceState();
        int64_t now_us = esp_timer_get_time();

        // Refresh user idle timer when entering listening from another state
        // (e.g. wake word just detected), so it won't immediately timeout.
        if (state != last_observed_state_) {
            if (state == kDeviceStateListening) {
                last_voice_activity_us_ = now_us;
            }
            last_observed_state_ = state;
        }

        bool is_user_speaking = app.IsVoiceDetected();
        bool user_idle_timed_out = (now_us - last_voice_activity_us_) >= (int64_t)HANDS_FREE_IDLE_TIMEOUT_MS * 1000;

        // Push-to-talk (GPIO3) must not be interrupted by hands-free timer logic.
        if (boot_button_pressed_) {
            last_voice_activity_us_ = now_us;
            wait_for_wake_word_ = false;
            return;
        }

        // Only user voice (VAD) refreshes inactivity timer.
        if (is_user_speaking) {
            last_voice_activity_us_ = now_us;
            wait_for_wake_word_ = false;
            return;
        }

        // Keep channel state while device is speaking or still playing audio.
        // Do not refresh user inactivity timer from playback/TTS activity.
        if (state == kDeviceStateSpeaking || !app.GetAudioService().IsIdle()) {
            wait_for_wake_word_ = false;
            return;
        }

        // In listening but no voice for timeout => close channel and go LOW_POWER.
        if (state == kDeviceStateListening) {
            if (app.IsWifiResetConfirmationPending()) {
                last_voice_activity_us_ = now_us;
                return;
            }
            if (user_idle_timed_out) {
                int64_t idle_ms = (now_us - last_voice_activity_us_) / 1000;
                // ESP log formatting on this target may not print %lld reliably.
                ESP_LOGI(TAG, "Hands-free idle timeout %d ms (idle=%ld ms), entering low-power standby",
                         HANDS_FREE_IDLE_TIMEOUT_MS, static_cast<long>(idle_ms));
                app.ToggleChatState();  // listening -> close channel -> idle -> LOW_POWER
#if CONFIG_USE_ESP_WAKE_WORD
                wait_for_wake_word_ = true;
                ShowWakeWordStandbyHint();
#else
                // Fallback if wake word feature is disabled.
                wait_for_wake_word_ = false;
#endif
                last_hands_free_trigger_us_ = now_us;
            }
            return;
        }

        // Idle state:
        // - if waiting for wake word, do not auto reopen channel.
        // - if not waiting, keep hands-free active by opening channel.
        if (state == kDeviceStateIdle) {
            if (user_idle_timed_out) {
#if CONFIG_USE_ESP_WAKE_WORD
                if (!wait_for_wake_word_) {
                    wait_for_wake_word_ = true;
                    ShowWakeWordStandbyHint();
                }
#endif
                // Keep device in low-power standby after user idle timeout.
                // Listening will resume only on explicit trigger (wake word / button).
                return;
            }

            if (wait_for_wake_word_) {
                return;
            }

            if (now_us - last_hands_free_trigger_us_ < (int64_t)HANDS_FREE_AUTO_LISTEN_RETRY_MS * 1000) {
                return;
            }

            last_hands_free_trigger_us_ = now_us;
            app.ToggleChatState();  // idle -> listening(auto mode)
            return;
        }

        // Any non-idle/non-listening state resets wake-word wait gate.
        wait_for_wake_word_ = false;
#endif
    }

    void InitializeHandsFreeMode() {
#if HANDS_FREE_AUTO_LISTEN
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                auto* board = static_cast<Esp32c3Inmp441Board*>(arg);
                board->TryStartHandsFreeListening();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "hands_free_listen",
            .skip_unhandled_events = true
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &hands_free_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(
            hands_free_timer_,
            (uint64_t)HANDS_FREE_AUTO_LISTEN_INTERVAL_MS * 1000));
        last_voice_activity_us_ = esp_timer_get_time();
        ESP_LOGI(TAG, "Hands-free enabled (interval=%d ms, retry=%d ms, idle_timeout=%d ms)",
                 HANDS_FREE_AUTO_LISTEN_INTERVAL_MS,
                 HANDS_FREE_AUTO_LISTEN_RETRY_MS,
                 HANDS_FREE_IDLE_TIMEOUT_MS);
#if !CONFIG_USE_ESP_WAKE_WORD
        ESP_LOGW(TAG, "Wake-word feature is disabled; standby wake by voice command is not available");
#endif
#else
        ESP_LOGI(TAG, "Hands-free auto-listen disabled, using push-to-talk on GPIO %d", BOOT_BUTTON_GPIO);
#endif
    }

public:
    Esp32c3Inmp441Board()
        : boot_button_(BOOT_BUTTON_GPIO),
          reset_ssid_button_(RESET_SSID_BUTTON_GPIO, false, RESET_SSID_LONG_PRESS_MS) {
        ESP_LOGI(TAG, "Initializing ESP32-C3 INMP441 Board with OLED");
        ESP_LOGI(TAG, "  BCLK: GPIO %d", AUDIO_I2S_GPIO_BCLK);
        ESP_LOGI(TAG, "  WS:   GPIO %d", AUDIO_I2S_GPIO_WS);
        ESP_LOGI(TAG, "  DIN:  GPIO %d (Mic)", AUDIO_I2S_GPIO_DIN);
        ESP_LOGI(TAG, "  DOUT: GPIO %d (Speaker)", AUDIO_I2S_GPIO_DOUT);
        ESP_LOGI(TAG, "  Button: GPIO %d", BOOT_BUTTON_GPIO);
        ESP_LOGI(TAG, "  GPIO2 Button: GPIO %d hands-free toggle (click), reset SSID (long press: %d ms)",
                 RESET_SSID_BUTTON_GPIO, RESET_SSID_LONG_PRESS_MS);
        ESP_LOGI(TAG, "  OLED: SDA=%d, SCL=%d (%dx%d)", 
                 DISPLAY_SDA_PIN, DISPLAY_SCL_PIN, DISPLAY_WIDTH, DISPLAY_HEIGHT);

        if (ModeStore::Current() == BootMode::Xiaozhi) {
            mode_selector_ = std::make_unique<ModeSelector>(
                BootMode::Xiaozhi, GetDisplay(),
                [this]() {
                    auto& app = Application::GetInstance();
                    if (app.GetDeviceState() == kDeviceStateStarting) {
                        EnterWifiConfigMode();
                    } else {
                        app.ToggleChatState();
                    }
                },
                []() {
                    auto& app = Application::GetInstance();
                    auto state = app.GetDeviceState();
                    if (state == kDeviceStateListening || state == kDeviceStateSpeaking) {
                        app.ToggleChatState();
                    }
                });
            InitializeButtons();
            InitializeHandsFreeMode();
        }
    }

    void OnListeningAutoStop() override {
        if (!hands_free_enabled_) {
            return;
        }
        int64_t now_us = esp_timer_get_time();
        last_hands_free_trigger_us_ = now_us;
        last_voice_activity_us_ = now_us;
        wait_for_wake_word_ = true;
#if CONFIG_USE_ESP_WAKE_WORD
        ShowWakeWordStandbyHint();
#endif
    }

    virtual AudioCodec* GetAudioCodec() override {
        // NoAudioCodecDuplex for INMP441 mic + MAX98357A speaker
        // Uses same I2S bus with shared BCLK/WS
        static NoAudioCodecDuplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN
        );
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return GetEsp32c3SharedOled(false);
    }

    bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        // Battery monitor removed — not used in this build
        level = 0;
        charging = false;
        discharging = false;
        return false;
    }
};

DECLARE_BOARD(Esp32c3Inmp441Board);
