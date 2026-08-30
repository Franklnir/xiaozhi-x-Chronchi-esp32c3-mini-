#ifndef XIAOZHI_CHRONOS_STATE_H_
#define XIAOZHI_CHRONOS_STATE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

struct ChronosNotification {
    uint8_t icon = 0;
    char title[33] = {};
    char message[129] = {};
};

struct ChronosWeatherDay {
    uint8_t icon = 0;
    int8_t temperature = 0;
    int8_t high = 0;
    int8_t low = 0;
};

struct ChronosNavigation {
    bool active = false;
    bool is_navigation = false;
    char title[49] = {};
    char duration[25] = {};
    char distance[25] = {};
    char eta[25] = {};
    char directions[97] = {};
    char speed[25] = {};
};

struct ChronosSnapshot {
    uint32_t revision = 0;
    bool connected = false;
    bool subscribed = false;
    bool time_valid = false;
    bool use_24_hour = true;
    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
    int64_t time_sync_us = 0;
    bool phone_battery_valid = false;
    bool phone_charging = false;
    uint8_t phone_battery = 0;
    std::array<ChronosNotification, 3> notifications = {};
    uint8_t notification_count = 0;
    char weather_city[33] = {};
    std::array<ChronosWeatherDay, 7> weather = {};
    uint8_t weather_count = 0;
    ChronosNavigation navigation = {};
};

class ChronosState {
public:
    void SetConnection(bool connected);
    void SetSubscribed(bool subscribed);
    bool ApplyFrame(const uint8_t* data, size_t length);
    ChronosSnapshot Snapshot() const;

private:
    static void CopyText(char* destination, size_t capacity,
                         const uint8_t* source, size_t length);
    static bool ReadCString(const uint8_t* data, size_t length, size_t* cursor,
                            char* destination, size_t capacity);

    mutable std::mutex mutex_;
    ChronosSnapshot state_;
    uint32_t notification_events_ = 0;
};

#endif  // XIAOZHI_CHRONOS_STATE_H_
