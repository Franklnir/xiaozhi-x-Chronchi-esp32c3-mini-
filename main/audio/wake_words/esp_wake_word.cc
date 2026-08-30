#include "esp_wake_word.h"
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <cstring>
#include "settings.h"


#define TAG "EspWakeWord"

EspWakeWord::EspWakeWord() {
}

EspWakeWord::~EspWakeWord() {
    running_ = false;
    std::lock_guard<std::mutex> lock(mutex_);
    if (wakenet_data_ != nullptr) {
        wakenet_iface_->destroy(wakenet_data_);
        wakenet_data_ = nullptr;
    }
    if (owns_model_list_ && wakenet_model_ != nullptr) {
        esp_srmodel_deinit(wakenet_model_);
        wakenet_model_ = nullptr;
    }
}

bool EspWakeWord::Initialize(AudioCodec* codec, srmodel_list_t* models_list) {
    std::lock_guard<std::mutex> lock(mutex_);
    codec_ = codec;

    if (wakenet_model_ == nullptr) {
        if (models_list == nullptr) {
            wakenet_model_ = esp_srmodel_init("model");
            owns_model_list_ = true;
        } else {
            wakenet_model_ = models_list;
            owns_model_list_ = false;
        }
    }

    if (wakenet_data_ != nullptr) {
        return true;
    }

    if (wakenet_model_ == nullptr || wakenet_model_->num == -1) {
        ESP_LOGE(TAG, "Failed to initialize wakenet model");
        return false;
    }
    // Pilihan wake word dapat dikonfigurasi via NVS ("audio"/"wake_word")
    Settings audio_settings("audio", false);
    std::string preferred = audio_settings.GetString("wake_word", "hijason");
    if (preferred != "hijason" && preferred != "hilexin" &&
        preferred != "hiesp" && preferred != "nihaoxiaozhi") {
        ESP_LOGW(TAG, "Invalid wake-word setting '%s'; falling back to hijason",
                 preferred.c_str());
        preferred = "hijason";
    }

    int selected_model_index = 0;
    if (wakenet_model_->num > 1) {
        ESP_LOGW(TAG, "More than one model found, selecting preferred wake word: %s", preferred.c_str());
        bool found = false;
        for (int i = 0; i < wakenet_model_->num; ++i) {
            const char* candidate = wakenet_model_->model_name[i];
            if (candidate != nullptr && std::strstr(candidate, preferred.c_str()) != nullptr) {
                selected_model_index = i;
                found = true;
                break;
            }
        }
        if (!found) {
            ESP_LOGW(TAG, "Preferred model '%s' is absent; using bundled model index 0",
                     preferred.c_str());
        }
    } else if (wakenet_model_->num == 0) {
        ESP_LOGE(TAG, "No model found");
        return false;
    }
    char *model_name = wakenet_model_->model_name[selected_model_index];
    if (model_name == nullptr) {
        ESP_LOGE(TAG, "Selected wake word model has no name");
        return false;
    }
    ESP_LOGI(TAG, "Selected wake word model: %s (%d/%d)", model_name, selected_model_index + 1, wakenet_model_->num);
    wakenet_iface_ = (esp_wn_iface_t*)esp_wn_handle_from_name(model_name);
    if (wakenet_iface_ == nullptr) {
        ESP_LOGE(TAG, "No WakeNet interface for model: %s", model_name);
        return false;
    }
    ESP_LOGI(TAG, "Creating WakeNet (free=%u, largest=%u)",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
    wakenet_data_ = wakenet_iface_->create(model_name, DET_MODE_95);
    if (wakenet_data_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create WakeNet model: %s", model_name);
        return false;
    }

    int frequency = wakenet_iface_->get_samp_rate(wakenet_data_);
    int audio_chunksize = wakenet_iface_->get_samp_chunksize(wakenet_data_);
    ESP_LOGI(TAG, "Wake word(%s),freq: %d, chunksize: %d, free=%u, largest=%u",
             model_name, frequency, audio_chunksize,
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));

    return true;
}

void EspWakeWord::OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) {
    wake_word_detected_callback_ = callback;
}

void EspWakeWord::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = wakenet_data_ != nullptr;
}

void EspWakeWord::Stop() {
    running_ = false;
    std::lock_guard<std::mutex> lock(mutex_);
    if (wakenet_data_ != nullptr) {
        wakenet_iface_->destroy(wakenet_data_);
        wakenet_data_ = nullptr;
        ESP_LOGI(TAG, "Released WakeNet runtime (free=%u, largest=%u)",
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
    }
}

void EspWakeWord::Feed(const std::vector<int16_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (wakenet_data_ == nullptr || !running_) {
        return;
    }

    int res = wakenet_iface_->detect(wakenet_data_, (int16_t *)data.data());
    if (res > 0) {
        last_detected_wake_word_ = wakenet_iface_->get_word_name(wakenet_data_, res);
        running_ = false;

        if (wake_word_detected_callback_) {
            wake_word_detected_callback_(last_detected_wake_word_);
        }
    }
}

size_t EspWakeWord::GetFeedSize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (wakenet_data_ == nullptr) {
        return 0;
    }
    return wakenet_iface_->get_samp_chunksize(wakenet_data_);
}

void EspWakeWord::EncodeWakeWordData() {
}

bool EspWakeWord::GetWakeWordOpus(std::vector<uint8_t>& opus) {
    return false;
}
