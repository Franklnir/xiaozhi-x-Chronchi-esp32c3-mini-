#include "display.h"

#define TAG "Display"

Display::Display() {
}

Display::~Display() {
}

void Display::SetStatus(const char* status) {
    ESP_LOGI(TAG, "SetStatus: %s", status ? status : "(null)");
}

void Display::ShowNotification(const char* notification, int duration_ms) {
    ESP_LOGI(TAG, "ShowNotification: %s (duration=%d)", notification ? notification : "(null)", duration_ms);
}

void Display::ShowNotification(const std::string &notification, int duration_ms) {
    ShowNotification(notification.c_str(), duration_ms);
}

void Display::SetEmotion(const char* emotion) {
    ESP_LOGI(TAG, "SetEmotion: %s", emotion ? emotion : "(null)");
}

void Display::SetChatMessage(const char* role, const char* content) {
    ESP_LOGI(TAG, "SetChatMessage: role=%s content=%s",
             role ? role : "(null)", content ? content : "(null)");
}

void Display::SetTheme(Theme* theme) {
    current_theme_ = theme;
}

void Display::UpdateStatusBar(bool update_all) {
}

void Display::SetPowerSaveMode(bool on) {
    ESP_LOGW(TAG, "SetPowerSaveMode: %d", on);
}

void Display::ShowModeMenu(int selected_mode) {
    static const char* labels[] = {"Mode: > Xiaozhi", "Mode: > Chronchi", "Mode: > Xichi"};
    int idx = (selected_mode >= 0 && selected_mode <= 2) ? selected_mode : 0;
    ShowNotification(labels[idx], 10000);
}

void Display::HideModeMenu() {
}

void Display::ShowModeSwitching(const char* mode_name) {
    std::string message = "Switching to ";
    message += mode_name;
    ShowNotification(message, 3000);
}

void Display::SetChronchiScreen(const ChronchiScreen& screen) {
}
