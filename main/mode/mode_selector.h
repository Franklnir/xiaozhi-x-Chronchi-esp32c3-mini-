#ifndef XIAOZHI_MODE_SELECTOR_H_
#define XIAOZHI_MODE_SELECTOR_H_

#include "boot_mode.h"

#include <esp_timer.h>

#include <functional>
#include <mutex>

class Display;

class ModeSelector {
public:
    static constexpr int kShortClickMs = 700;
    static constexpr int kDoubleClickWindowMs = 400;
    static constexpr int kEnterMenuMs = 2500;
    static constexpr int kConfirmMs = 2000;
    static constexpr int kMenuTimeoutMs = 10000;

    ModeSelector(BootMode current_mode, Display* display,
                 std::function<void()> short_click,
                 std::function<void()> before_menu = {});
    ~ModeSelector();

    void OnPressDown();
    void OnPressUp();
    bool IsMenuActive() const;

private:
    static void TimerCallback(void* arg);
    static void RestartCallback(void* arg);
    void Tick();
    void Confirm(BootMode selected);

    BootMode current_mode_;
    BootMode selected_mode_;
    Display* display_;
    std::function<void()> short_click_;
    std::function<void()> before_menu_;
    esp_timer_handle_t timer_ = nullptr;
    esp_timer_handle_t restart_timer_ = nullptr;

    mutable std::mutex mutex_;
    bool pressed_ = false;
    bool menu_active_ = false;
    bool must_release_ = false;
    bool confirm_fired_ = false;
    bool pending_single_click_ = false;
    int64_t press_started_us_ = 0;
    int64_t last_release_us_ = 0;
    int64_t menu_deadline_us_ = 0;
};

#endif  // XIAOZHI_MODE_SELECTOR_H_
