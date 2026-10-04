#include "mode_selector.h"

#include "display/display.h"
#include "mode_store.h"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_system.h>

#include <utility>

namespace {
constexpr char kTag[] = "ModeButton";
constexpr int64_t MsToUs(int ms) { return static_cast<int64_t>(ms) * 1000; }
}  // namespace

ModeSelector::ModeSelector(BootMode current_mode, Display* display,
                           std::function<void()> short_click,
                           std::function<void()> long_press)
    : current_mode_(current_mode),
      display_(display),
      short_click_(std::move(short_click)),
      long_press_(std::move(long_press)) {
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = TimerCallback;
    timer_args.arg = this;
    timer_args.dispatch_method = ESP_TIMER_TASK;
    timer_args.name = "mode_button";
    timer_args.skip_unhandled_events = true;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_, 50 * 1000));

    esp_timer_create_args_t restart_args = {};
    restart_args.callback = RestartCallback;
    restart_args.arg = this;
    restart_args.dispatch_method = ESP_TIMER_TASK;
    restart_args.name = "mode_restart";
    ESP_ERROR_CHECK(esp_timer_create(&restart_args, &restart_timer_));
}

ModeSelector::~ModeSelector() {
    if (timer_ != nullptr) {
        esp_timer_stop(timer_);
        esp_timer_delete(timer_);
    }
    if (restart_timer_ != nullptr) {
        esp_timer_stop(restart_timer_);
        esp_timer_delete(restart_timer_);
    }
}

void ModeSelector::OnPressDown() {
    bool double_click = false;
    BootMode target_mode = BootMode::Xiaozhi;
    const int64_t now = esp_timer_get_time();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (switching_mode_ || pressed_) {
            return;
        }

        // Fast double-click: second press down within window of first press release
        if (pending_single_click_) {
            const int64_t time_since_release_us = now - last_release_us_;
            if (time_since_release_us <= MsToUs(kDoubleClickWindowMs)) {
                pending_single_click_ = false;
                double_click = true;
                target_mode = (current_mode_ == BootMode::Xiaozhi) ? BootMode::Chronchi : BootMode::Xiaozhi;
                switching_mode_ = true;
            } else {
                pending_single_click_ = false;
            }
        }

        pressed_ = true;
        long_press_fired_ = false;
        press_started_us_ = now;
    }

    if (double_click) {
        ESP_LOGI(kTag, "Double-click on GPIO 3 detected! Switching mode from %s to %s",
                 BootModeName(current_mode_), BootModeName(target_mode));
        Confirm(target_mode);
    }
}

void ModeSelector::OnPressUp() {
    const int64_t now = esp_timer_get_time();

    std::lock_guard<std::mutex> lock(mutex_);
    if (!pressed_) {
        return;
    }
    pressed_ = false;
    const int64_t held_us = now - press_started_us_;

    if (switching_mode_) {
        return;
    }

    if (long_press_fired_) {
        long_press_fired_ = false;
        pending_single_click_ = false;
        return;
    }

    if (held_us < MsToUs(kShortClickMs)) {
        // Arm pending single-click. If a 2nd click arrives within kDoubleClickWindowMs,
        // OnPressDown triggers double-click mode switch. If window expires, Tick fires short_click_.
        pending_single_click_ = true;
        last_release_us_ = now;
    } else {
        pending_single_click_ = false;
    }
}

void ModeSelector::TimerCallback(void* arg) {
    static_cast<ModeSelector*>(arg)->Tick();
}

void ModeSelector::RestartCallback(void*) {
    esp_restart();
}

void ModeSelector::Tick() {
    std::function<void()> fire_single_click;
    std::function<void()> fire_long_press;
    const int64_t now = esp_timer_get_time();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (switching_mode_) {
            return;
        }

        // Fire pending single click if double-click window expired without a second press
        if (pending_single_click_ && !pressed_) {
            if (now - last_release_us_ >= MsToUs(kDoubleClickWindowMs)) {
                pending_single_click_ = false;
                fire_single_click = short_click_;
            }
        }

        // Fire long press when button held >= kLongPressMs (2.5s)
        if (pressed_ && !long_press_fired_ &&
            now - press_started_us_ >= MsToUs(kLongPressMs)) {
            pending_single_click_ = false;
            long_press_fired_ = true;
            fire_long_press = long_press_;
        }
    }

    if (fire_single_click) {
        fire_single_click();
    }

    if (fire_long_press) {
        ESP_LOGI(kTag, "Long press on GPIO 3 triggered (held >= %d ms)", kLongPressMs);
        fire_long_press();
    }
}

void ModeSelector::Confirm(BootMode selected) {
    if (selected == current_mode_) {
        ESP_LOGI(kTag, "Mode unchanged: %s", BootModeName(selected));
        if (display_ != nullptr) {
            display_->ShowNotification("Mode unchanged", 1500);
        }
        std::lock_guard<std::mutex> lock(mutex_);
        switching_mode_ = false;
        return;
    }

    esp_err_t err = ModeStore::Save(selected);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to save mode: %s", esp_err_to_name(err));
        if (display_ != nullptr) {
            display_->ShowNotification("Mode save failed", 3000);
        }
        std::lock_guard<std::mutex> lock(mutex_);
        switching_mode_ = false;
        return;
    }

    if (display_ != nullptr) {
        display_->ShowModeSwitching(BootModeName(selected));
    }
    esp_timer_stop(restart_timer_);
    ESP_ERROR_CHECK(esp_timer_start_once(restart_timer_, 700 * 1000));
}
