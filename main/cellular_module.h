#ifndef CELLULAR_MODULE_H
#define CELLULAR_MODULE_H

#include <string>
#include <functional>
#include <driver/uart.h>

enum CellularStatus {
    CELLULAR_DISCONNECTED,
    CELLULAR_CONNECTING,
    CELLULAR_CONNECTED,
    CELLULAR_ERROR
};

enum CellularModuleType {
    MODULE_ML307R,
    MODULE_EC801E,
    MODULE_NT26
};

class CellularModule {
public:
    static CellularModule& GetInstance() {
        static CellularModule instance;
        return instance;
    }
    
    // Initialize cellular module
    bool Initialize(CellularModuleType type, int tx_pin, int rx_pin);
    void Deinitialize();
    
    // Connection management
    bool Connect();
    void Disconnect();
    CellularStatus GetStatus() const { return status_; }
    
    // Network info
    std::string GetOperator();
    int GetSignalStrength();  // 0-31, 99=unknown
    bool IsSimReady();
    
    // Data connection
    bool IsDataConnected();
    std::string GetIpAddress();
    
    // SMS (optional)
    bool SendSms(const std::string& number, const std::string& message);
    
    // Callbacks
    using StatusCallback = std::function<void(CellularStatus status)>;
    void SetStatusCallback(StatusCallback callback) { status_callback_ = callback; }

private:
    CellularModule();
    ~CellularModule();
    
    bool SendAtCommand(const std::string& cmd, std::string& response, int timeout_ms = 1000);
    bool WaitForResponse(const std::string& expected, int timeout_ms);
    void ProcessResponse(const std::string& response);
    
    CellularModuleType module_type_;
    CellularStatus status_ = CELLULAR_DISCONNECTED;
    uart_port_t uart_port_ = UART_NUM_1;
    StatusCallback status_callback_ = nullptr;
    bool initialized_ = false;
    
    // Network info
    std::string operator_name_;
    int signal_strength_ = 0;
    std::string ip_address_;
};

#endif // CELLULAR_MODULE_H
