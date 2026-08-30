#ifndef XIAOZHI_CHRONCHI_MODELS_H_
#define XIAOZHI_CHRONCHI_MODELS_H_

#include <cstdint>

enum class ChronchiScreenType : uint8_t {
    Startup,
    Connection,
    Home,
    Message,
    Professional,
    Payment,
    Order,
    Navigation,
    System,
};

enum class ChronchiConnectionState : uint8_t {
    Waiting,
    Connected,
    Disconnected,
    Reconnecting,
};

enum class ChronchiPaymentDirection : uint8_t {
    Incoming,
    Outgoing,
    Unknown,
};

enum class ChronchiOrderStatus : uint8_t {
    Confirmed,
    Packed,
    Shipped,
    InTransit,
    OutForDelivery,
    Delivered,
    Cancelled,
    Unknown,
};

enum class ChronchiManeuver : uint8_t {
    Straight,
    Left,
    Right,
    SlightLeft,
    SlightRight,
    Roundabout,
    Arrive,
    Unknown,
};

// Fixed-size display model. BLE input never owns pointers and never allocates
// strings that outlive the parser callback.
struct ChronchiScreen {
    ChronchiScreenType type = ChronchiScreenType::Home;
    ChronchiConnectionState connection = ChronchiConnectionState::Waiting;
    ChronchiPaymentDirection payment_direction = ChronchiPaymentDirection::Unknown;
    ChronchiOrderStatus order_status = ChronchiOrderStatus::Unknown;
    ChronchiManeuver maneuver = ChronchiManeuver::Unknown;

    char source_app[25] = {};
    char time[9] = {};
    char primary[65] = {};
    char secondary[129] = {};
    char footer[65] = {};
    char date[33] = {};
    char weather[17] = {};
    char location[25] = {};
    char network[13] = {};

    uint8_t battery = 0;
    uint8_t signal = 0;
    uint8_t notification_count = 0;
    bool wifi = false;
    bool charging = false;
    bool battery_valid = false;
};

struct ChronchiSnapshot {
    uint32_t revision = 0;
    bool connected = false;
    bool subscribed = false;
    bool navigation_active = false;
    ChronchiScreen active = {};
};

#endif  // XIAOZHI_CHRONCHI_MODELS_H_
