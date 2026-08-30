#ifndef WEB_SETTINGS_SERVER_H
#define WEB_SETTINGS_SERVER_H

#include <string>
#include <functional>
#include <esp_http_server.h>

class WebSettingsServer {
public:
    static WebSettingsServer& GetInstance() {
        static WebSettingsServer instance;
        return instance;
    }
    
    // Start/stop the web server
    bool Start(uint16_t port = 80);
    void Stop();
    
    // Check if server is running
    bool IsRunning() const { return server_ != nullptr; }

private:
    WebSettingsServer();
    ~WebSettingsServer();
    
    // HTTP handlers
    static esp_err_t HandleRoot(httpd_req_t* req);
    static esp_err_t HandleGetConfig(httpd_req_t* req);
    static esp_err_t HandleSetConfig(httpd_req_t* req);
    static esp_err_t HandleAdvancedConfig(httpd_req_t* req);
    static esp_err_t HandleSaveAdvanced(httpd_req_t* req);
    static esp_err_t HandleScanWifi(httpd_req_t* req);
    static esp_err_t HandleConnectWifi(httpd_req_t* req);
    static esp_err_t HandleResetWifi(httpd_req_t* req);
    static esp_err_t HandleExportSettings(httpd_req_t* req);
    static esp_err_t HandleImportSettings(httpd_req_t* req);
    static esp_err_t HandleReboot(httpd_req_t* req);
    
    // Generate HTML pages
    std::string GenerateMainPage();
    std::string GenerateAdvancedPage();
    
    httpd_handle_t server_ = nullptr;
};

#endif // WEB_SETTINGS_SERVER_H
