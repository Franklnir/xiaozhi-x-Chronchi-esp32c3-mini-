#include "cellular_module.h"
#include <esp_log.h>
#include <cstring>

#define TAG "Cellular"
#define BUF_SIZE 1024

CellularModule::CellularModule() {}
CellularModule::~CellularModule() { Deinitialize(); }

bool CellularModule::Initialize(CellularModuleType type, int tx_pin, int rx_pin) {
    if (initialized_) return true;
    
    module_type_ = type;
    
    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    esp_err_t err = uart_param_config(uart_port_, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART config failed: %s", esp_err_to_name(err));
        return false;
    }
    
    err = uart_set_pin(uart_port_, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART pin config failed: %s", esp_err_to_name(err));
        return false;
    }
    
    err = uart_driver_install(uart_port_, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(err));
        return false;
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "Cellular module initialized (type=%d, tx=%d, rx=%d)", type, tx_pin, rx_pin);
    
    // Test AT command
    std::string response;
    if (SendAtCommand("AT", response, 1000)) {
        ESP_LOGI(TAG, "Module responded: %s", response.c_str());
    }
    
    return true;
}

void CellularModule::Deinitialize() {
    if (initialized_) {
        uart_driver_delete(uart_port_);
        initialized_ = false;
    }
}

bool CellularModule::SendAtCommand(const std::string& cmd, std::string& response, int timeout_ms) {
    if (!initialized_) return false;
    
    // Send command
    std::string full_cmd = cmd + "\r\n";
    uart_write_bytes(uart_port_, full_cmd.c_str(), full_cmd.length());
    
    // Read response
    char buf[BUF_SIZE];
    int len = uart_read_bytes(uart_port_, buf, BUF_SIZE - 1, pdMS_TO_TICKS(timeout_ms));
    if (len > 0) {
        buf[len] = '\0';
        response = buf;
        return response.find("OK") != std::string::npos;
    }
    
    return false;
}

bool CellularModule::Connect() {
    if (!initialized_) return false;
    
    status_ = CELLULAR_CONNECTING;
    if (status_callback_) status_callback_(status_);
    
    std::string response;
    
    // Check SIM
    if (!IsSimReady()) {
        ESP_LOGE(TAG, "SIM not ready");
        status_ = CELLULAR_ERROR;
        if (status_callback_) status_callback_(status_);
        return false;
    }
    
    // Enable radio
    SendAtCommand("AT+CFUN=1", response, 5000);
    
    // Wait for network registration
    for (int i = 0; i < 30; i++) {
        if (SendAtCommand("AT+CREG?", response, 1000)) {
            if (response.find(",1") != std::string::npos || response.find(",5") != std::string::npos) {
                ESP_LOGI(TAG, "Network registered");
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // Activate PDP context
    SendAtCommand("AT+CGDCONT=1,\"IP\",\"internet\"", response, 1000);
    SendAtCommand("AT+CGACT=1,1", response, 5000);
    
    // Check data connection
    if (IsDataConnected()) {
        status_ = CELLULAR_CONNECTED;
        ESP_LOGI(TAG, "Cellular connected");
    } else {
        status_ = CELLULAR_ERROR;
        ESP_LOGE(TAG, "Failed to connect");
    }
    
    if (status_callback_) status_callback_(status_);
    return status_ == CELLULAR_CONNECTED;
}

void CellularModule::Disconnect() {
    std::string response;
    SendAtCommand("AT+CGACT=0,1", response, 1000);
    status_ = CELLULAR_DISCONNECTED;
    if (status_callback_) status_callback_(status_);
}

bool CellularModule::IsSimReady() {
    std::string response;
    if (SendAtCommand("AT+CPIN?", response, 1000)) {
        return response.find("READY") != std::string::npos;
    }
    return false;
}

std::string CellularModule::GetOperator() {
    std::string response;
    if (SendAtCommand("AT+COPS?", response, 1000)) {
        // Parse operator name from response
        size_t pos = response.find("\"");
        if (pos != std::string::npos) {
            size_t end = response.find("\"", pos + 1);
            if (end != std::string::npos) {
                operator_name_ = response.substr(pos + 1, end - pos - 1);
            }
        }
    }
    return operator_name_;
}

int CellularModule::GetSignalStrength() {
    std::string response;
    if (SendAtCommand("AT+CSQ", response, 1000)) {
        // Parse RSSI from +CSQ: <rssi>,<ber>
        size_t pos = response.find("+CSQ:");
        if (pos != std::string::npos) {
            int rssi = atoi(response.substr(pos + 6).c_str());
            signal_strength_ = rssi;
            return rssi;
        }
    }
    return 99;  // Unknown
}

bool CellularModule::IsDataConnected() {
    std::string response;
    if (SendAtCommand("AT+CGACT?", response, 1000)) {
        return response.find("1,1") != std::string::npos;
    }
    return false;
}

std::string CellularModule::GetIpAddress() {
    std::string response;
    if (SendAtCommand("AT+CGPADDR=1", response, 1000)) {
        // Parse IP from +CGPADDR: 1,"x.x.x.x"
        size_t pos = response.find("\"");
        if (pos != std::string::npos) {
            size_t end = response.find("\"", pos + 1);
            if (end != std::string::npos) {
                ip_address_ = response.substr(pos + 1, end - pos - 1);
            }
        }
    }
    return ip_address_;
}

bool CellularModule::SendSms(const std::string& number, const std::string& message) {
    std::string response;
    
    // Set text mode
    SendAtCommand("AT+CMGF=1", response, 1000);
    
    // Set SMS number
    std::string cmd = "AT+CMGS=\"" + number + "\"";
    uart_write_bytes(uart_port_, cmd.c_str(), cmd.length());
    uart_write_bytes(uart_port_, "\r\n", 2);
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Send message body
    uart_write_bytes(uart_port_, message.c_str(), message.length());
    
    // Send Ctrl+Z to end
    char ctrl_z = 0x1A;
    uart_write_bytes(uart_port_, &ctrl_z, 1);
    
    return WaitForResponse("OK", 10000);
}

bool CellularModule::WaitForResponse(const std::string& expected, int timeout_ms) {
    char buf[BUF_SIZE];
    int len = uart_read_bytes(uart_port_, buf, BUF_SIZE - 1, pdMS_TO_TICKS(timeout_ms));
    if (len > 0) {
        buf[len] = '\0';
        std::string response(buf);
        return response.find(expected) != std::string::npos;
    }
    return false;
}
