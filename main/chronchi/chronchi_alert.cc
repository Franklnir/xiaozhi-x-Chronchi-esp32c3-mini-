#include "chronchi_alert.h"
#include "boards/esp32c3-inmp441/config.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <cmath>
#include <cstring>

namespace {
constexpr char kTag[] = "ChronchiAlert";
constexpr uint32_t kSampleRate = 24000;
constexpr size_t kChunkSamples = 256;
constexpr float kTwoPi = 6.28318530717958647692f;
}  // namespace

ChronchiAlert& ChronchiAlert::GetInstance() {
    static ChronchiAlert instance;
    return instance;
}

ChronchiAlert::ChronchiAlert() = default;

ChronchiAlert::~ChronchiAlert() {
    StopAlert();
    if (tx_handle_ != nullptr) {
        i2s_channel_disable(tx_handle_);
        i2s_del_channel(tx_handle_);
        tx_handle_ = nullptr;
    }
}

void ChronchiAlert::Initialize(gpio_num_t vibration_pin) {
    vibration_pin_ = vibration_pin;
    if (vibration_pin_ != GPIO_NUM_NC && GPIO_IS_VALID_OUTPUT_GPIO(vibration_pin_)) {
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << vibration_pin_);
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        esp_err_t err = gpio_config(&io_conf);
        if (err == ESP_OK) {
            gpio_set_level(vibration_pin_, 0);
            ESP_LOGI(kTag, "Vibration motor initialized on GPIO %d", vibration_pin_);
        } else {
            ESP_LOGW(kTag, "Failed to configure vibration GPIO %d: %s", vibration_pin_, esp_err_to_name(err));
        }
    } else {
        ESP_LOGI(kTag, "Vibration motor pin not configured or invalid (NC)");
    }
}

void ChronchiAlert::SetVibration(bool enable) {
    if (vibration_pin_ != GPIO_NUM_NC && GPIO_IS_VALID_OUTPUT_GPIO(vibration_pin_)) {
        gpio_set_level(vibration_pin_, enable ? 1 : 0);
    }
}

void ChronchiAlert::VibratePattern(uint32_t on_ms, uint32_t off_ms, uint8_t repeat) {
    if (vibration_pin_ == GPIO_NUM_NC || !GPIO_IS_VALID_OUTPUT_GPIO(vibration_pin_)) {
        return;
    }

    struct VibrateParam {
        gpio_num_t pin;
        uint32_t on;
        uint32_t off;
        uint8_t rep;
    };

    auto* param = new VibrateParam{vibration_pin_, on_ms, off_ms, repeat};
    xTaskCreate([](void* arg) {
        auto* p = static_cast<VibrateParam*>(arg);
        for (uint8_t i = 0; i < p->rep; i++) {
            gpio_set_level(p->pin, 1);
            vTaskDelay(pdMS_TO_TICKS(p->on));
            gpio_set_level(p->pin, 0);
            if (i + 1 < p->rep && p->off > 0) {
                vTaskDelay(pdMS_TO_TICKS(p->off));
            }
        }
        delete p;
        vTaskDelete(nullptr);
    }, "vib_once", 2048, param, 3, nullptr);
}

void ChronchiAlert::VibrateNotification() {
    ESP_LOGI(kTag, "Notification received: triggering double vibration pulse");
    // 2x getar pendek (100ms ON, 80ms OFF, 100ms ON)
    VibratePattern(100, 80, 2);
}

void ChronchiAlert::EnsureAudioInitialized() {
    if (audio_initialized_.load()) {
        return;
    }

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 2,
        .dma_frame_num = 128,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    esp_err_t err = i2s_new_channel(&chan_cfg, &tx_handle_, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to create I2S TX channel: %s", esp_err_to_name(err));
        return;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = kSampleRate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
            .ws_width = I2S_DATA_BIT_WIDTH_32BIT,
            .ws_pol = false,
            .bit_shift = true,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = AUDIO_I2S_GPIO_BCLK,
            .ws = AUDIO_I2S_GPIO_WS,
            .dout = AUDIO_I2S_GPIO_DOUT,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };

    err = i2s_channel_init_std_mode(tx_handle_, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to init I2S std mode: %s", esp_err_to_name(err));
        i2s_del_channel(tx_handle_);
        tx_handle_ = nullptr;
        return;
    }

    err = i2s_channel_enable(tx_handle_);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to enable I2S channel: %s", esp_err_to_name(err));
        return;
    }

#if defined(AUDIO_ES8311_PA_PIN) && (AUDIO_ES8311_PA_PIN != GPIO_NUM_NC)
    gpio_set_level(AUDIO_ES8311_PA_PIN, 1);
#endif

    audio_initialized_.store(true);
    ESP_LOGI(kTag, "I2S Audio Bell ready (DOUT=GPIO%d, BCLK=GPIO%d, WS=GPIO%d)",
             AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS);
}

void ChronchiAlert::StartFindDeviceAlarm() {
    if (alarm_active_.load()) {
        ESP_LOGI(kTag, "Find device alarm already running");
        return;
    }

    ESP_LOGW(kTag, "=== FIND DEVICE TRIGGERED: Ringing loud bell and pulsing vibration! ===");
    alarm_active_.store(true);
    EnsureAudioInitialized();

    xTaskCreate(AlertTask, "find_alert", 3072, this, 5, &alert_task_handle_);
}

void ChronchiAlert::StartCallAlert() {
    StartFindDeviceAlarm();
}

void ChronchiAlert::StopAlert() {
    if (!alarm_active_.load()) {
        return;
    }

    ESP_LOGI(kTag, "Stopping alert / bell ringing");
    alarm_active_.store(false);
    SetVibration(false);
    alert_task_handle_ = nullptr;
}

void ChronchiAlert::AlertTask(void* param) {
    auto* self = static_cast<ChronchiAlert*>(param);
    const int64_t start_time_us = esp_timer_get_time();
    constexpr int64_t kTimeoutUs = 30LL * 1000 * 1000;  // 30 detik timeout otomatis

    int32_t pcm_buffer[kChunkSamples];

    // Parameter Chime Bell Klasik Dua Nada ("Ding-Dong" bernada tinggi & nyaring)
    // Nada 1: 2048 Hz (G6) + 2560 Hz harmonik (Ding!)
    // Nada 2: 1706 Hz + 2133 Hz (Dong!)
    constexpr float f1_a = 2048.0f;
    constexpr float f1_b = 2560.0f;
    constexpr float f2_a = 1706.0f;
    constexpr float f2_b = 2133.0f;

    constexpr uint32_t kStrike1DurationSamples = (kSampleRate * 22) / 100; // 220ms
    constexpr uint32_t kStrike2DurationSamples = (kSampleRate * 28) / 100; // 280ms
    constexpr uint32_t kPauseDurationSamples   = (kSampleRate * 15) / 100; // 150ms
    constexpr uint32_t kTotalCycleSamples      = kStrike1DurationSamples + kStrike2DurationSamples + kPauseDurationSamples;

    while (self->alarm_active_.load()) {
        if (esp_timer_get_time() - start_time_us > kTimeoutUs) {
            ESP_LOGI(kTag, "Alarm timed out after 30 seconds");
            break;
        }

        uint32_t sample_in_cycle = 0;
        self->SetVibration(true);  // Getar saat bell mulai berdentang

        while (sample_in_cycle < kTotalCycleSamples && self->alarm_active_.load()) {
            size_t chunk_count = std::min<size_t>(kChunkSamples, kTotalCycleSamples - sample_in_cycle);

            for (size_t i = 0; i < chunk_count; i++) {
                uint32_t current_sample = sample_in_cycle + i;
                float tone = 0.0f;

                if (current_sample < kStrike1DurationSamples) {
                    // Strike 1: Nada Ding (Tinggi & Menggelegar)
                    float t = static_cast<float>(current_sample) / static_cast<float>(kSampleRate);
                    float env = std::exp(-t * 11.0f); // Peluruhan eksponensial khas bell
                    tone = (std::sin(kTwoPi * f1_a * t) * 0.60f + std::sin(kTwoPi * f1_b * t) * 0.40f) * env;
                } else if (current_sample < kStrike1DurationSamples + kStrike2DurationSamples) {
                    // Strike 2: Nada Dong
                    uint32_t rel_sample = current_sample - kStrike1DurationSamples;
                    float t = static_cast<float>(rel_sample) / static_cast<float>(kSampleRate);
                    float env = std::exp(-t * 9.0f);
                    tone = (std::sin(kTwoPi * f2_a * t) * 0.60f + std::sin(kTwoPi * f2_b * t) * 0.40f) * env;
                } else {
                    // Jeda sejenak sebelum dentangan berikutnya
                    tone = 0.0f;
                }

                // Skala maksimal 100% digital volume (amplitude 32767 << 16 untuk I2S 32-bit slot)
                int32_t val_16 = static_cast<int32_t>(tone * 32767.0f);
                if (val_16 > 32767) val_16 = 32767;
                if (val_16 < -32767) val_16 = -32767;
                pcm_buffer[i] = val_16 << 16;
            }

            // Tulis audio langsung ke speaker MAX98357A
            if (self->tx_handle_ != nullptr) {
                size_t bytes_written = 0;
                i2s_channel_write(self->tx_handle_, pcm_buffer, chunk_count * sizeof(int32_t),
                                  &bytes_written, pdMS_TO_TICKS(100));
            }

            sample_in_cycle += chunk_count;

            // Matikan getar di paruh akhir siklus untuk menghasilkan efek denyut berirama
            if (sample_in_cycle >= (kStrike1DurationSamples + kStrike2DurationSamples)) {
                self->SetVibration(false);
            }
        }

        self->SetVibration(false);
        // Delay singkat 100ms sebelum dentangan bell siklus berikutnya
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    self->SetVibration(false);
    self->alarm_active_.store(false);
    ESP_LOGI(kTag, "AlertTask finished");
    vTaskDelete(nullptr);
}
