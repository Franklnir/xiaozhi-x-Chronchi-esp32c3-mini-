#include "settings_manager.h"
#include <esp_log.h>
#include <cJSON.h>
#include <cstring>

#define TAG "SettingsManager"
#define NVS_NAMESPACE "xiaozhi"
#define MAX_WIFI_CREDENTIALS 10

SettingsManager::SettingsManager() {
    settings_.wifi_max_ssids = 3;
    settings_.wifi_remember_bssid = true;
    settings_.wifi_max_tx_power = 20;
    settings_.sleep_mode_enabled = false;
    settings_.sleep_timeout_sec = 60;
    settings_.volume = 50;
    settings_.wake_word = "hilexin";
    settings_.oled_mode = "classic";
    settings_.ota_url = "https://api.tenclass.net/xiaozhi/ota/";
    settings_.language = "id_ID";
}

SettingsManager::~SettingsManager() {
    if (nvs_handle_) {
        nvs_close(nvs_handle_);
    }
}

bool SettingsManager::Initialize() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(err));
            return false;
        }
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init NVS: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }
    
    if (!LoadFromNvs()) {
        ESP_LOGW(TAG, "Failed to load settings, using defaults");
        ResetToDefaults();
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "Settings manager initialized");
    return true;
}

bool SettingsManager::LoadFromNvs() {
    auto load_string = [this](const char* key, std::string& destination) {
        size_t length = 0;
        if (nvs_get_str(nvs_handle_, key, nullptr, &length) != ESP_OK || length == 0) {
            return;
        }
        std::string buffer(length, '\0');
        if (nvs_get_str(nvs_handle_, key, buffer.data(), &length) == ESP_OK) {
            destination.assign(buffer.c_str());
        }
    };
    load_string("wake_word", settings_.wake_word);
    load_string("oled_mode", settings_.oled_mode);
    load_string("ota_url", settings_.ota_url);
    load_string("language", settings_.language);

    int8_t i8_value = 0;
    int16_t i16_value = 0;
    uint8_t u8_value = 0;
    if (nvs_get_i8(nvs_handle_, "volume", &i8_value) == ESP_OK) {
        settings_.volume = i8_value;
    }
    if (nvs_get_u8(nvs_handle_, "sleep_en", &u8_value) == ESP_OK) {
        settings_.sleep_mode_enabled = u8_value != 0;
    }
    if (nvs_get_i16(nvs_handle_, "sleep_sec", &i16_value) == ESP_OK) {
        settings_.sleep_timeout_sec = i16_value;
    }
    if (nvs_get_u8(nvs_handle_, "bssid_en", &u8_value) == ESP_OK) {
        settings_.wifi_remember_bssid = u8_value != 0;
    }
    if (nvs_get_i8(nvs_handle_, "tx_power", &i8_value) == ESP_OK) {
        settings_.wifi_max_tx_power = i8_value;
    }
    
    // Load WiFi credentials
    LoadWifiCredentials();
    
    return true;
}

bool SettingsManager::Save() {
    if (!initialized_) return false;
    
    esp_err_t err;
    
    err = nvs_set_str(nvs_handle_, "wake_word", settings_.wake_word.c_str());
    err |= nvs_set_str(nvs_handle_, "oled_mode", settings_.oled_mode.c_str());
    err |= nvs_set_str(nvs_handle_, "ota_url", settings_.ota_url.c_str());
    err |= nvs_set_str(nvs_handle_, "language", settings_.language.c_str());
    err |= nvs_set_i8(nvs_handle_, "volume", settings_.volume);
    err |= nvs_set_u8(nvs_handle_, "sleep_en", settings_.sleep_mode_enabled);
    err |= nvs_set_i16(nvs_handle_, "sleep_sec", settings_.sleep_timeout_sec);
    err |= nvs_set_u8(nvs_handle_, "bssid_en", settings_.wifi_remember_bssid);
    err |= nvs_set_i8(nvs_handle_, "tx_power", settings_.wifi_max_tx_power);
    
    err |= SaveWifiCredentials();
    
    err |= nvs_commit(nvs_handle_);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save settings: %s", esp_err_to_name(err));
        return false;
    }
    
    // Notify callbacks
    for (auto& cb : callbacks_) {
        cb("all");
    }
    
    return true;
}

bool SettingsManager::SaveWifiCredentials() {
    cJSON* array = cJSON_CreateArray();
    for (const auto& cred : settings_.wifi_credentials) {
        cJSON* obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "ssid", cred.ssid.c_str());
        cJSON_AddStringToObject(obj, "pass", cred.password.c_str());
        cJSON_AddStringToObject(obj, "bssid", cred.bssid.c_str());
        cJSON_AddNumberToObject(obj, "prio", cred.priority);
        cJSON_AddItemToArray(array, obj);
    }
    
    char* json = cJSON_PrintUnformatted(array);
    esp_err_t err = nvs_set_str(nvs_handle_, "wifi_creds", json);
    
    free(json);
    cJSON_Delete(array);
    
    return err == ESP_OK;
}

bool SettingsManager::LoadWifiCredentials() {
    size_t len = 0;
    esp_err_t err = nvs_get_str(nvs_handle_, "wifi_creds", NULL, &len);
    if (err != ESP_OK || len == 0) return false;
    
    char* json = (char*)malloc(len);
    err = nvs_get_str(nvs_handle_, "wifi_creds", json, &len);
    if (err != ESP_OK) {
        free(json);
        return false;
    }
    
    cJSON* array = cJSON_Parse(json);
    free(json);
    
    if (!cJSON_IsArray(array)) {
        cJSON_Delete(array);
        return false;
    }
    
    settings_.wifi_credentials.clear();
    cJSON* item = NULL;
    cJSON_ArrayForEach(item, array) {
        WifiCredential cred;
        cJSON* ssid = cJSON_GetObjectItem(item, "ssid");
        cJSON* pass = cJSON_GetObjectItem(item, "pass");
        cJSON* bssid = cJSON_GetObjectItem(item, "bssid");
        cJSON* prio = cJSON_GetObjectItem(item, "prio");
        
        if (cJSON_IsString(ssid)) cred.ssid = ssid->valuestring;
        if (cJSON_IsString(pass)) cred.password = pass->valuestring;
        if (cJSON_IsString(bssid)) cred.bssid = bssid->valuestring;
        if (cJSON_IsNumber(prio)) cred.priority = prio->valueint;
        
        settings_.wifi_credentials.push_back(cred);
    }
    
    cJSON_Delete(array);
    return true;
}

bool SettingsManager::AddWifiCredential(const std::string& ssid, const std::string& password, int priority) {
    // Check if SSID already exists
    for (auto& cred : settings_.wifi_credentials) {
        if (cred.ssid == ssid) {
            cred.password = password;
            if (priority >= 0) cred.priority = priority;
            Save();
            return true;
        }
    }
    
    // Check max limit
    if (settings_.wifi_credentials.size() >= MAX_WIFI_CREDENTIALS) {
        ESP_LOGW(TAG, "Max WiFi credentials reached");
        return false;
    }
    
    WifiCredential cred;
    cred.ssid = ssid;
    cred.password = password;
    cred.priority = (priority >= 0) ? priority : settings_.wifi_credentials.size();
    
    settings_.wifi_credentials.push_back(cred);
    Save();
    
    ESP_LOGI(TAG, "Added WiFi credential: %s", ssid.c_str());
    return true;
}

bool SettingsManager::RemoveWifiCredential(const std::string& ssid) {
    for (auto it = settings_.wifi_credentials.begin(); it != settings_.wifi_credentials.end(); ++it) {
        if (it->ssid == ssid) {
            settings_.wifi_credentials.erase(it);
            Save();
            ESP_LOGI(TAG, "Removed WiFi credential: %s", ssid.c_str());
            return true;
        }
    }
    return false;
}

bool SettingsManager::UpdateWifiCredential(const std::string& ssid, const std::string& password) {
    for (auto& cred : settings_.wifi_credentials) {
        if (cred.ssid == ssid) {
            cred.password = password;
            Save();
            return true;
        }
    }
    return false;
}

std::vector<WifiCredential> SettingsManager::GetWifiCredentials() const {
    return settings_.wifi_credentials;
}

WifiCredential* SettingsManager::GetWifiCredentialBySsid(const std::string& ssid) {
    for (auto& cred : settings_.wifi_credentials) {
        if (cred.ssid == ssid) {
            return &cred;
        }
    }
    return nullptr;
}

bool SettingsManager::SaveBssid(const std::string& ssid, const std::string& bssid) {
    if (!settings_.wifi_remember_bssid) return false;
    
    for (auto& cred : settings_.wifi_credentials) {
        if (cred.ssid == ssid) {
            cred.bssid = bssid;
            Save();
            return true;
        }
    }
    return false;
}

std::string SettingsManager::GetBssid(const std::string& ssid) {
    for (const auto& cred : settings_.wifi_credentials) {
        if (cred.ssid == ssid) {
            return cred.bssid;
        }
    }
    return "";
}

// Getters and setters
std::string SettingsManager::GetWakeWord() const { return settings_.wake_word; }
void SettingsManager::SetWakeWord(const std::string& wake_word) {
    settings_.wake_word = wake_word;
    Save();
}

std::string SettingsManager::GetOledMode() const { return settings_.oled_mode; }
void SettingsManager::SetOledMode(const std::string& mode) {
    settings_.oled_mode = mode;
    Save();
}

bool SettingsManager::IsSleepModeEnabled() const { return settings_.sleep_mode_enabled; }
void SettingsManager::SetSleepModeEnabled(bool enabled) {
    settings_.sleep_mode_enabled = enabled;
    Save();
}

int SettingsManager::GetSleepTimeout() const { return settings_.sleep_timeout_sec; }
void SettingsManager::SetSleepTimeout(int seconds) {
    settings_.sleep_timeout_sec = seconds;
    Save();
}

std::string SettingsManager::GetOtaUrl() const { return settings_.ota_url; }
void SettingsManager::SetOtaUrl(const std::string& url) {
    settings_.ota_url = url;
    Save();
}

int SettingsManager::GetVolume() const { return settings_.volume; }
void SettingsManager::SetVolume(int volume) {
    settings_.volume = volume;
    Save();
}

std::string SettingsManager::GetLanguage() const { return settings_.language; }
void SettingsManager::SetLanguage(const std::string& lang) {
    settings_.language = lang;
    Save();
}

int SettingsManager::GetWifiMaxTxPower() const { return settings_.wifi_max_tx_power; }
void SettingsManager::SetWifiMaxTxPower(int power) {
    settings_.wifi_max_tx_power = power;
    Save();
}

bool SettingsManager::ResetToDefaults() {
    settings_.wake_word = "hilexin";
    settings_.oled_mode = "classic";
    settings_.ota_url = "https://api.tenclass.net/xiaozhi/ota/";
    settings_.language = "id_ID";
    settings_.volume = 50;
    settings_.sleep_mode_enabled = false;
    settings_.sleep_timeout_sec = 60;
    settings_.wifi_remember_bssid = true;
    settings_.wifi_max_tx_power = 20;
    settings_.wifi_max_ssids = 3;
    settings_.wifi_credentials.clear();
    
    return Save();
}

std::string SettingsManager::ExportToJson() const {
    cJSON* root = cJSON_CreateObject();
    
    cJSON_AddStringToObject(root, "wake_word", settings_.wake_word.c_str());
    cJSON_AddStringToObject(root, "oled_mode", settings_.oled_mode.c_str());
    cJSON_AddStringToObject(root, "ota_url", settings_.ota_url.c_str());
    cJSON_AddStringToObject(root, "language", settings_.language.c_str());
    cJSON_AddNumberToObject(root, "volume", settings_.volume);
    cJSON_AddBoolToObject(root, "sleep_mode", settings_.sleep_mode_enabled);
    cJSON_AddNumberToObject(root, "sleep_timeout", settings_.sleep_timeout_sec);
    cJSON_AddBoolToObject(root, "remember_bssid", settings_.wifi_remember_bssid);
    cJSON_AddNumberToObject(root, "tx_power", settings_.wifi_max_tx_power);
    
    cJSON* wifi_array = cJSON_CreateArray();
    for (const auto& cred : settings_.wifi_credentials) {
        cJSON* obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "ssid", cred.ssid.c_str());
        cJSON_AddStringToObject(obj, "password", cred.password.c_str());
        cJSON_AddNumberToObject(obj, "priority", cred.priority);
        cJSON_AddItemToArray(wifi_array, obj);
    }
    cJSON_AddItemToObject(root, "wifi", wifi_array);
    
    char* json = cJSON_PrintUnformatted(root);
    std::string result(json);
    free(json);
    cJSON_Delete(root);
    
    return result;
}

bool SettingsManager::ImportFromJson(const std::string& json) {
    cJSON* root = cJSON_Parse(json.c_str());
    if (!root) return false;
    
    cJSON* item;
    
    item = cJSON_GetObjectItem(root, "wake_word");
    if (cJSON_IsString(item)) settings_.wake_word = item->valuestring;
    
    item = cJSON_GetObjectItem(root, "oled_mode");
    if (cJSON_IsString(item)) settings_.oled_mode = item->valuestring;
    
    item = cJSON_GetObjectItem(root, "ota_url");
    if (cJSON_IsString(item)) settings_.ota_url = item->valuestring;
    
    item = cJSON_GetObjectItem(root, "language");
    if (cJSON_IsString(item)) settings_.language = item->valuestring;
    
    item = cJSON_GetObjectItem(root, "volume");
    if (cJSON_IsNumber(item)) settings_.volume = item->valueint;
    
    item = cJSON_GetObjectItem(root, "sleep_mode");
    if (cJSON_IsBool(item)) settings_.sleep_mode_enabled = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(root, "sleep_timeout");
    if (cJSON_IsNumber(item)) settings_.sleep_timeout_sec = item->valueint;
    
    item = cJSON_GetObjectItem(root, "remember_bssid");
    if (cJSON_IsBool(item)) settings_.wifi_remember_bssid = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(root, "tx_power");
    if (cJSON_IsNumber(item)) settings_.wifi_max_tx_power = item->valueint;
    
    // Import WiFi credentials
    cJSON* wifi_array = cJSON_GetObjectItem(root, "wifi");
    if (cJSON_IsArray(wifi_array)) {
        settings_.wifi_credentials.clear();
        cJSON* wifi_item = NULL;
        cJSON_ArrayForEach(wifi_item, wifi_array) {
            cJSON* ssid = cJSON_GetObjectItem(wifi_item, "ssid");
            cJSON* pass = cJSON_GetObjectItem(wifi_item, "password");
            cJSON* prio = cJSON_GetObjectItem(wifi_item, "priority");
            
            if (cJSON_IsString(ssid) && cJSON_IsString(pass)) {
                WifiCredential cred;
                cred.ssid = ssid->valuestring;
                cred.password = pass->valuestring;
                cred.priority = cJSON_IsNumber(prio) ? prio->valueint : settings_.wifi_credentials.size();
                settings_.wifi_credentials.push_back(cred);
            }
        }
    }
    
    cJSON_Delete(root);
    return Save();
}

void SettingsManager::RegisterChangeCallback(SettingsChangeCallback callback) {
    callbacks_.push_back(callback);
}
