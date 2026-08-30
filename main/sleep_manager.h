#ifndef SLEEP_MANAGER_H
#define SLEEP_MANAGER_H

#include <esp_sleep.h>
#include <esp_timer.h>

class SleepManager {
public:
    static SleepManager& GetInstance() {
        static SleepManager instance;
        return instance;
    }
    
    // Initialize sleep manager
    bool Initialize();
    
    // Enable/disable sleep mode
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return enabled_; }
    
    // Set timeout in seconds
    void SetTimeout(int seconds);
    int GetTimeout() const { return timeout_sec_; }
    
    // Reset activity timer (call on any user interaction)
    void ResetActivity();
    
    // Check if should sleep and enter if needed
    void CheckAndSleep();
    
    // Enter light sleep immediately
    void EnterLightSleep();
    
    // Configure wake sources
    void ConfigureWakeSources();

private:
    SleepManager();
    ~SleepManager();
    
    bool enabled_ = false;
    int timeout_sec_ = 60;
    int64_t last_activity_us_ = 0;
    bool configured_ = false;
};

#endif // SLEEP_MANAGER_H
