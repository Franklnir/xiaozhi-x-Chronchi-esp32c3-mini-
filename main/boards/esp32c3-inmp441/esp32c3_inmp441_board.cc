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
#include "codecs/es8311_audio_codec.h"
#include "display/display.h"
#include "application.h"
#include "adc_battery_monitor.h"
#include "button.h"
#include "config.h"
#include "shared_oled.h"
#include "mode/mode_selector.h"
#include "mode/mode_store.h"
#include "settings.h"
#include "youtube_audio_poller.h"

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
    std::unique_ptr<Button> reset_ssid_button_;
    std::unique_ptr<ModeSelector> mode_selector_;
    esp_timer_handle_t hands_free_timer_ = nullptr;
    int64_t last_hands_free_trigger_us_ = 0;
    int64_t last_voice_activity_us_ = 0;
    bool hands_free_enabled_ = true;
    bool wait_for_wake_word_ = false;
    bool boot_button_pressed_ = false;
    bool youtube_interrupted_ = false;
    DeviceState last_observed_state_ = kDeviceStateUnknown;

    bool is_es8311_detected_ = false;
    uint8_t es8311_addr_ = AUDIO_ES8311_I2C_ADDR;

    void DetectAudioCodec() {
        i2c_master_bus_handle_t i2c_bus = GetEsp32c3SharedI2cBus();
        if (i2c_bus != nullptr) {
            // Probe ES8311 at address 0x18 (default), fallback to 0x19
            if (i2c_master_probe(i2c_bus, AUDIO_ES8311_I2C_ADDR, 50) == ESP_OK) {
                is_es8311_detected_ = true;
                es8311_addr_ = AUDIO_ES8311_I2C_ADDR;
            } else if (i2c_master_probe(i2c_bus, 0x19, 50) == ESP_OK) {
                is_es8311_detected_ = true;
                es8311_addr_ = 0x19;
            }
        }
        if (is_es8311_detected_) {
            ESP_LOGI(TAG, "Audio hardware detected: WeAct ES8311 + NS4150B (I2C addr: 0x%02x)", es8311_addr_);
        } else {
            ESP_LOGI(TAG, "Audio hardware detected: INMP441 + MAX98357A (NoAudioCodecDuplex)");
        }
    }

    void ShowWakeWordStandbyHint() {
        Settings audio_settings("audio", false);
        std::string selected = audio_settings.GetString("wake_word", "hijason");
        if (selected.empty() || (selected != "hijason" && selected != "hilexin" &&
            selected != "hiesp" && selected != "nihaoxiaozhi")) {
            Settings xz_settings("xiaozhi", false);
            selected = xz_settings.GetString("wake_word", "hijason");
        }
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
            if (YouTubeAudioPoller::GetInstance().IsPlaying()) {
                ESP_LOGI(TAG, "GPIO 3 pressed while playing YouTube -> Instant Abort (0 ms)!");
                youtube_interrupted_ = true;
                YouTubeAudioPoller::GetInstance().Abort();
            }
            mode_selector_->OnPressDown();
        });
        boot_button_.OnPressUp([this]() {
            boot_button_pressed_ = false;
            mode_selector_->OnPressUp();
        });

        // Only enable reset_ssid_button_ if it's not being used as PA pin
        if (!is_es8311_detected_ || AUDIO_ES8311_PA_PIN != RESET_SSID_BUTTON_GPIO) {
            reset_ssid_button_ = std::make_unique<Button>(RESET_SSID_BUTTON_GPIO, false, RESET_SSID_LONG_PRESS_MS);

            // GPIO2 single click toggles hands-free mode.
            reset_ssid_button_->OnClick([this]() {
                ToggleHandsFreeMode();
            });

            // Hold reset button to clear saved SSID/password and reboot.
            reset_ssid_button_->OnLongPress([this]() {
                ESP_LOGW(TAG, "Reset SSID button long pressed");

                if (!ClearStoredWifiCredentials()) {
                    GetDisplay()->ShowNotification("Failed to reset SSID");
                    return;
                }

                GetDisplay()->ShowNotification("SSID reset, rebooting...");
                esp_restart();
            });
        } else {
            ESP_LOGI(TAG, "GPIO %d is assigned to NS4150B PA_CTRL, secondary button disabled", RESET_SSID_BUTTON_GPIO);
        }
    }

    void TryStartHandsFreeListening() {
#if HANDS_FREE_AUTO_LISTEN
        if (YouTubeAudioPoller::GetInstance().IsPlaying()) {
            return;
        }
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
        : boot_button_(BOOT_BUTTON_GPIO) {
        DetectAudioCodec();

        ESP_LOGI(TAG, "Initializing ESP32-C3 Board (%s)",
                 is_es8311_detected_ ? "WeAct ES8311+NS4150B" : "INMP441+MAX98357A");
        ESP_LOGI(TAG, "  BCLK: GPIO %d", AUDIO_I2S_GPIO_BCLK);
        ESP_LOGI(TAG, "  WS:   GPIO %d", AUDIO_I2S_GPIO_WS);
        ESP_LOGI(TAG, "  DIN:  GPIO %d (Mic)", AUDIO_I2S_GPIO_DIN);
        ESP_LOGI(TAG, "  DOUT: GPIO %d (Speaker)", AUDIO_I2S_GPIO_DOUT);
        if (is_es8311_detected_) {
            ESP_LOGI(TAG, "  MCLK: GPIO %d", AUDIO_I2S_GPIO_MCLK);
            ESP_LOGI(TAG, "  PA:   GPIO %d", AUDIO_ES8311_PA_PIN);
        }
        ESP_LOGI(TAG, "  Button: GPIO %d", BOOT_BUTTON_GPIO);
        if (!is_es8311_detected_ || AUDIO_ES8311_PA_PIN != RESET_SSID_BUTTON_GPIO) {
            ESP_LOGI(TAG, "  GPIO2 Button: GPIO %d hands-free toggle (click), reset SSID (long press: %d ms)",
                     RESET_SSID_BUTTON_GPIO, RESET_SSID_LONG_PRESS_MS);
        } else {
            ESP_LOGI(TAG, "  GPIO 2: Dedicated to NS4150B PA_CTRL");
        }
        ESP_LOGI(TAG, "  OLED: SDA=%d, SCL=%d (%dx%d)", 
                 DISPLAY_SDA_PIN, DISPLAY_SCL_PIN, DISPLAY_WIDTH, DISPLAY_HEIGHT);

        if (ModeStore::Current() == BootMode::Xiaozhi) {
            mode_selector_ = std::make_unique<ModeSelector>(
                BootMode::Xiaozhi, GetDisplay(),
                [this]() {
                    if (youtube_interrupted_) {
                        youtube_interrupted_ = false;
                        auto display = GetDisplay();
                        if (display != nullptr) {
                            display->ShowNotification("Lagu dihentikan", 2000);
                        }
                        return;
                    }
                    auto& app = Application::GetInstance();
                    last_voice_activity_us_ = esp_timer_get_time();
                    wait_for_wake_word_ = false;
                    if (app.GetDeviceState() != kDeviceStateStarting) {
                        app.ToggleChatState();
                    }
                },
                [this]() {
                    youtube_interrupted_ = false;
                    ESP_LOGI(TAG, "GPIO 3 Long Press -> Entering WiFi Config Mode");
                    EnterWifiConfigMode();
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
        static AudioCodec* audio_codec = nullptr;
        if (audio_codec != nullptr) {
            return audio_codec;
        }

        if (is_es8311_detected_) {
            i2c_master_bus_handle_t i2c_bus = GetEsp32c3SharedI2cBus();
            audio_codec = new Es8311AudioCodec(
                i2c_bus,
                I2C_NUM_0,
                AUDIO_INPUT_SAMPLE_RATE,
                AUDIO_OUTPUT_SAMPLE_RATE,
                AUDIO_I2S_GPIO_MCLK,
                AUDIO_I2S_GPIO_BCLK,
                AUDIO_I2S_GPIO_WS,
                AUDIO_I2S_GPIO_DOUT,
                AUDIO_I2S_GPIO_DIN,
                AUDIO_ES8311_PA_PIN,
                es8311_addr_,
                true,   // use_mclk
                false   // pa_inverted
            );
        } else {
            audio_codec = new NoAudioCodecDuplex(
                AUDIO_INPUT_SAMPLE_RATE,
                AUDIO_OUTPUT_SAMPLE_RATE,
                AUDIO_I2S_GPIO_BCLK,
                AUDIO_I2S_GPIO_WS,
                AUDIO_I2S_GPIO_DOUT,
                AUDIO_I2S_GPIO_DIN
            );
        }
        return audio_codec;
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
