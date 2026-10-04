#include "chronchi_alert.h"
#include "boards/esp32c3-inmp441/config.h"
#include "boards/esp32c3-inmp441/shared_oled.h"
#include "codecs/es8311_audio_codec.h"
#include "codecs/no_audio_codec.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <driver/i2c_master.h>
#include <cmath>
#include <cstring>
#include <mutex>
#include <algorithm>

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
    if (audio_codec_ != nullptr) {
        delete audio_codec_;
        audio_codec_ = nullptr;
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

    // Hangatkan subsistem audio agar respon seketika saat ada notifikasi atau alarm
    EnsureAudioInitialized();
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

void ChronchiAlert::EnsureAudioInitialized() {
    if (audio_initialized_.load()) {
        return;
    }

    static std::mutex init_mutex;
    std::lock_guard<std::mutex> lock(init_mutex);
    if (audio_initialized_.load()) {
        return;
    }

    i2c_master_bus_handle_t i2c_bus = GetEsp32c3SharedI2cBus();
    bool is_es8311_detected = false;
    uint8_t es8311_addr = AUDIO_ES8311_I2C_ADDR;

    if (i2c_bus != nullptr) {
        // Cek apakah modul WeAct ES8311 terpasang di I2C address 0x18 (default) atau 0x19
        if (i2c_master_probe(i2c_bus, AUDIO_ES8311_I2C_ADDR, 50) == ESP_OK) {
            is_es8311_detected = true;
            es8311_addr = AUDIO_ES8311_I2C_ADDR;
        } else if (i2c_master_probe(i2c_bus, 0x19, 50) == ESP_OK) {
            is_es8311_detected = true;
            es8311_addr = 0x19;
        }
    }

    if (is_es8311_detected) {
        ESP_LOGI(kTag, "Audio hardware: WeAct Studio ES8311 DAC + NS4150B PA (addr: 0x%02x, MCLK=GPIO%d, PA=GPIO%d)",
                 es8311_addr, AUDIO_I2S_GPIO_MCLK, AUDIO_ES8311_PA_PIN);
        audio_codec_ = new Es8311AudioCodec(
            i2c_bus,
            I2C_NUM_0,
            kSampleRate,
            kSampleRate,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_ES8311_PA_PIN,
            es8311_addr,
            true,   // use_mclk = true (diperlukan ES8311)
            false   // pa_inverted = false
        );
    } else {
        ESP_LOGI(kTag, "Audio hardware: MAX98357A I2S Amplifier (BCLK=GPIO%d, WS=GPIO%d, DOUT=GPIO%d)",
                 AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT);
        audio_codec_ = new NoAudioCodecDuplex(
            kSampleRate,
            kSampleRate,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN
        );
    }

    if (audio_codec_ != nullptr) {
        audio_codec_->Start();
        audio_codec_->EnableInput(false);   // Mic tidak digunakan di Chronchi, hemat DMA & daya
        audio_codec_->SetOutputVolume(100); // Volume maksimal 100%
        audio_initialized_.store(true);
        ESP_LOGI(kTag, "Chronchi speaker audio ready at 100%% volume");
    } else {
        ESP_LOGE(kTag, "Failed to create audio codec for Chronchi");
    }
}

void ChronchiAlert::PlayNotificationSound() {
    EnsureAudioInitialized();
    if (audio_codec_ == nullptr) return;
    if (alarm_active_.load()) return; // Jangan tumpuk jika alarm dering sedang aktif

    xTaskCreate([](void* arg) {
        auto* self = static_cast<ChronchiAlert*>(arg);
        if (self->notif_busy_.exchange(true)) {
            vTaskDelete(nullptr);
            return;
        }

        // Suara "tit" notifikasi (70 ms, nada 2000 Hz, attack 2ms dan decay eksponensial bersih)
        constexpr uint32_t kTitSamples = (kSampleRate * 70) / 1000; // 1680 samples
        int16_t buf[kChunkSamples];
        uint32_t sample_idx = 0;

        while (sample_idx < kTitSamples && !self->alarm_active_.load()) {
            size_t chunk = std::min<size_t>(kChunkSamples, kTitSamples - sample_idx);
            for (size_t i = 0; i < chunk; i++) {
                uint32_t s = sample_idx + i;
                float t = static_cast<float>(s) / static_cast<float>(kSampleRate);

                // Nada "tit" bernada 2000 Hz dengan attack halus 2ms agar tidak ada bunyi "pop"
                float env = 1.0f;
                if (s < (kSampleRate * 2) / 1000) {
                    env = static_cast<float>(s) / (kSampleRate * 0.002f);
                } else {
                    float t_decay = static_cast<float>(s - (kSampleRate * 2) / 1000) / static_cast<float>(kSampleRate);
                    env = std::exp(-t_decay * 35.0f);
                }

                // Sintesis gelombang sinus 2000 Hz + harmonik 3000 Hz tipis agar nyaring di speaker kecil
                float raw = std::sin(kTwoPi * 2000.0f * t) * 0.85f + std::sin(kTwoPi * 3000.0f * t) * 0.15f;
                int32_t val = static_cast<int32_t>(raw * env * 31500.0f);
                if (val > 32767) val = 32767;
                if (val < -32767) val = -32767;
                buf[i] = static_cast<int16_t>(val);
            }

            if (self->audio_codec_ != nullptr) {
                self->audio_codec_->Output(buf, chunk);
            }
            sample_idx += chunk;
        }

        self->notif_busy_.store(false);
        vTaskDelete(nullptr);
    }, "notif_snd", 2048, this, 4, nullptr);
}

void ChronchiAlert::VibrateNotification() {
    ESP_LOGI(kTag, "Notification received: playing 'tit' chime and double vibration pulse");
    PlayNotificationSound();
    // 2x getar pendek (100ms ON, 80ms OFF, 100ms ON)
    VibratePattern(100, 80, 2);
}

void ChronchiAlert::StartFindDeviceAlarm() {
    if (alarm_active_.load()) {
        ESP_LOGI(kTag, "Find device alarm already active");
        return;
    }

    ESP_LOGW(kTag, "=== FIND DEVICE TRIGGERED: Ringing phone alarm and pulsating vibration! ===");
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

    ESP_LOGI(kTag, "Stopping alert / alarm ringing");
    alarm_active_.store(false);
    SetVibration(false);
    alert_task_handle_ = nullptr;
}

void ChronchiAlert::AlertTask(void* param) {
    auto* self = static_cast<ChronchiAlert*>(param);
    const int64_t start_time_us = esp_timer_get_time();
    constexpr int64_t kTimeoutUs = 30LL * 1000 * 1000;  // 30 detik timeout otomatis

    int16_t pcm_buffer[kChunkSamples];

    // Frekuensi warble dering telepon / alarm:
    // F1: 1568.0 Hz (G6)
    // F2: 2093.0 Hz (C7)
    // Warble bergantian setiap 25 ms (40 Hz warble rate) - suara khas telepon berdering keras
    constexpr float f1 = 1568.0f;
    constexpr float f2 = 2093.0f;
    constexpr uint32_t kWarbleSamples = (kSampleRate * 25) / 1000; // 25ms per warble step
    constexpr uint32_t kFadeSamples   = (kSampleRate * 5)  / 1000; // 5ms fade in/out anti-pop

    // Pola Dering Telepon (Double Ring Cycle):
    // Burst 1: 420 ms dering warble + getar ON
    // Gap:     140 ms jeda hening + getar OFF
    // Burst 2: 420 ms dering warble + getar ON
    // Pause:   720 ms jeda antar deringan + getar OFF
    // Total Siklus: 1700 ms (1.7 detik)
    constexpr uint32_t kBurst1Samples = (kSampleRate * 420) / 1000; // 10080
    constexpr uint32_t kGapSamples    = (kSampleRate * 140) / 1000; // 3360
    constexpr uint32_t kBurst2Samples = (kSampleRate * 420) / 1000; // 10080
    constexpr uint32_t kPauseSamples  = (kSampleRate * 720) / 1000; // 17280
    constexpr uint32_t kTotalCycleSamples = kBurst1Samples + kGapSamples + kBurst2Samples + kPauseSamples;

    while (self->alarm_active_.load()) {
        if (esp_timer_get_time() - start_time_us > kTimeoutUs) {
            ESP_LOGI(kTag, "Find device alarm timed out after 30 seconds");
            break;
        }

        uint32_t sample_in_cycle = 0;

        while (sample_in_cycle < kTotalCycleSamples && self->alarm_active_.load()) {
            size_t chunk_count = std::min<size_t>(kChunkSamples, kTotalCycleSamples - sample_in_cycle);

            // Tentukan status burst saat ini
            bool in_burst1 = (sample_in_cycle < kBurst1Samples);
            bool in_burst2 = (sample_in_cycle >= (kBurst1Samples + kGapSamples) &&
                              sample_in_cycle < (kBurst1Samples + kGapSamples + kBurst2Samples));
            bool is_active_burst = in_burst1 || in_burst2;

            self->SetVibration(is_active_burst);

            for (size_t i = 0; i < chunk_count; i++) {
                uint32_t s = sample_in_cycle + i;
                float tone = 0.0f;

                uint32_t burst_rel_s = 0;
                bool active = false;

                if (s < kBurst1Samples) {
                    burst_rel_s = s;
                    active = true;
                } else if (s >= (kBurst1Samples + kGapSamples) && s < (kBurst1Samples + kGapSamples + kBurst2Samples)) {
                    burst_rel_s = s - (kBurst1Samples + kGapSamples);
                    active = true;
                }

                if (active) {
                    // Warble bergantian F1 dan F2 tiap 25 ms
                    uint32_t warble_phase = (burst_rel_s / kWarbleSamples) % 2;
                    float freq = (warble_phase == 0) ? f1 : f2;
                    float t = static_cast<float>(s) / static_cast<float>(kSampleRate);

                    // Fade in 5ms dan Fade out 5ms di ujung burst agar tidak berderik/pop
                    float fade = 1.0f;
                    if (burst_rel_s < kFadeSamples) {
                        fade = static_cast<float>(burst_rel_s) / static_cast<float>(kFadeSamples);
                    } else if (burst_rel_s > (kBurst1Samples - kFadeSamples)) {
                        fade = static_cast<float>(kBurst1Samples - burst_rel_s) / static_cast<float>(kFadeSamples);
                    }

                    // Modulasi amplitudo 20Hz untuk rasa getar dering mekanik
                    float tremolo = 0.85f + 0.15f * std::sin(kTwoPi * 20.0f * t);
                    // Harmonik kedua untuk ketajaman di speaker micro
                    tone = (std::sin(kTwoPi * freq * t) * 0.80f + std::sin(kTwoPi * (freq * 1.5f) * t) * 0.20f) * tremolo * fade;
                } else {
                    tone = 0.0f;
                }

                int32_t val = static_cast<int32_t>(tone * 31500.0f);
                if (val > 32767) val = 32767;
                if (val < -32767) val = -32767;
                pcm_buffer[i] = static_cast<int16_t>(val);
            }

            if (self->audio_codec_ != nullptr) {
                self->audio_codec_->Output(pcm_buffer, chunk_count);
            }

            sample_in_cycle += chunk_count;
        }

        self->SetVibration(false);
    }

    self->SetVibration(false);
    self->alarm_active_.store(false);
    self->alert_task_handle_ = nullptr;
    ESP_LOGI(kTag, "Find device AlertTask finished");
    vTaskDelete(nullptr);
}
