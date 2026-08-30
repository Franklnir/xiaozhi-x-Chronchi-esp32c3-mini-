#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>

enum BatteryLevel {
    BATTERY_UNKNOWN = -1,
    BATTERY_EMPTY = 0,
    BATTERY_QUARTER = 1,
    BATTERY_HALF = 2,
    BATTERY_THREE_QUARTERS = 3,
    BATTERY_FULL = 4,
    BATTERY_CHARGING = 5
};

class BatteryMonitor {
public:
    static BatteryMonitor& GetInstance() {
        static BatteryMonitor instance;
        return instance;
    }
    
    bool Initialize(int adc_pin = 0);
    void Deinitialize();
    
    // Get battery voltage in millivolts
    int GetVoltageMv();
    
    // Get battery percentage (0-100)
    int GetPercentage();
    
    // Get battery level enum for display
    BatteryLevel GetLevel();
    
    // Check if charging (if supported)
    bool IsCharging();
    
    // Update battery reading (call periodically)
    void Update();

private:
    BatteryMonitor();
    ~BatteryMonitor();
    
    int ReadAdc();
    
    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    adc_cali_handle_t cali_handle_ = nullptr;
    int adc_pin_ = 0;
    int voltage_mv_ = 0;
    int percentage_ = 0;
    bool initialized_ = false;
    
    // Voltage thresholds (adjustable)
    int voltage_empty_ = 3200;   // 3.2V = empty
    int voltage_full_ = 4200;    // 4.2V = full
};

#endif // BATTERY_MONITOR_H
