#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <string>
#include <vector>
#include <functional>
#include <nvs_flash.h>
#include <nvs.h>

struct WifiCredential {
    std::string ssid;
    std::string password;
    std::string bssid;  // Optional: for faster reconnection
    int priority;       // Lower = higher priority
};

struct DeviceSettings {
    // WiFi settings
    std::vector<WifiCredential> wifi_credentials;
    int wifi_max_ssids;
    bool wifi_remember_bssid;
    int wifi_max_tx_power;
    
    // Wake word settings
    std::string wake_word;
    
    // Display settings
    std::string oled_mode;  // "classic" or "amoled"
    
    // Power settings
    bool sleep_mode_enabled;
    int sleep_timeout_sec;
    
    // OTA settings
    std::string ota_url;
    
    // Audio settings
    int volume;
    
    // Language
    std::string language;
};

class SettingsManager {
public:
    static SettingsManager& GetInstance() {
        static SettingsManager instance;
        return instance;
    }
    
    // Initialize NVS and load settings
    bool Initialize();
    
    // WiFi credential management
    bool AddWifiCredential(const std::string& ssid, const std::string& password, int priority = -1);
    bool RemoveWifiCredential(const std::string& ssid);
    bool UpdateWifiCredential(const std::string& ssid, const std::string& password);
    std::vector<WifiCredential> GetWifiCredentials() const;
    WifiCredential* GetWifiCredentialBySsid(const std::string& ssid);
    
    // BSSID management
    bool SaveBssid(const std::string& ssid, const std::string& bssid);
    std::string GetBssid(const std::string& ssid);
    
    // Settings getters/setters
    std::string GetWakeWord() const;
    void SetWakeWord(const std::string& wake_word);
    
    std::string GetOledMode() const;
    void SetOledMode(const std::string& mode);
    
    bool IsSleepModeEnabled() const;
    void SetSleepModeEnabled(bool enabled);
    
    int GetSleepTimeout() const;
    void SetSleepTimeout(int seconds);
    
    std::string GetOtaUrl() const;
    void SetOtaUrl(const std::string& url);
    
    int GetVolume() const;
    void SetVolume(int volume);
    
    std::string GetLanguage() const;
    void SetLanguage(const std::string& lang);
    
    int GetWifiMaxTxPower() const;
    void SetWifiMaxTxPower(int power);
    
    // Save all settings to NVS
    bool Save();
    
    // Reset to defaults
    bool ResetToDefaults();
    
    // Export/Import settings as JSON
    std::string ExportToJson() const;
    bool ImportFromJson(const std::string& json);
    
    // Callback for settings changes
    using SettingsChangeCallback = std::function<void(const std::string& key)>;
    void RegisterChangeCallback(SettingsChangeCallback callback);

private:
    SettingsManager();
    ~SettingsManager();
    
    bool LoadFromNvs();
    bool SaveWifiCredentials();
    bool LoadWifiCredentials();
    
    nvs_handle_t nvs_handle_ = 0;
    DeviceSettings settings_;
    std::vector<SettingsChangeCallback> callbacks_;
    bool initialized_ = false;
};

#endif // SETTINGS_MANAGER_H
