#ifndef XIAOZHI_CHRONCHI_STATE_H_
#define XIAOZHI_CHRONCHI_STATE_H_

#include "chronchi_models.h"

#include <cstddef>
#include <cstdint>
#include <mutex>

class ChronchiState {
public:
    ChronchiState();

    void ShowStartup();
    void SetConnection(bool connected);
    void SetSubscribed(bool subscribed);
    void UpdateClock(const char* time, const char* date);
    void Set24HourMode(bool hour24);
    void UpdatePhoneStatus(uint8_t battery, bool charging);
    void UpdateDeviceBattery(uint8_t battery, bool charging);
    void UpdateNetwork(const char* network, uint8_t signal, bool wifi);
    void UpdateWeather(const char* weather, const char* location);
    void UpdateWeatherHighLow(const char* high_low);
    void ApplyScreen(const ChronchiScreen& screen);
    void UpdateNavCustomIcon(const uint8_t* icon_32x32);
    void StopNavigation();
    void DismissTransient();
    void ShowSystem(const char* primary, const char* secondary);
    bool Tick();
    ChronchiSnapshot Snapshot() const;

    static void CopyText(char* destination, size_t capacity, const char* source);
    static uint8_t PriorityFor(ChronchiScreenType type);
    static uint32_t TimeoutFor(ChronchiScreenType type);

private:
    void SetActiveLocked(const ChronchiScreen& screen, int64_t now_us);
    void StampDeviceBatteryLocked(ChronchiScreen& screen) const;
    void RefreshHomeLocked();
    bool ActiveExpiredLocked(int64_t now_us) const;

    mutable std::mutex mutex_;
    ChronchiScreen home_ = {};
    ChronchiScreen active_ = {};
    uint32_t revision_ = 0;
    int64_t expires_us_ = 0;
    bool connected_ = false;
    bool subscribed_ = false;
    bool navigation_active_ = false;
    int64_t last_navigation_us_ = 0;
    ChronchiScreen saved_navigation_ = {};
    uint8_t device_battery_ = 0;
    bool device_charging_ = false;
    bool device_battery_valid_ = false;
    uint8_t phone_battery_ = 0;
    bool phone_charging_ = false;
    bool phone_battery_valid_ = false;
    bool hour24_ = true;
    char weather_high_low_[24] = {};
};

#endif  // XIAOZHI_CHRONCHI_STATE_H_
