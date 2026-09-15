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
constexpr int kModeCount = 2;  // Xiaozhi, Chronchi
}  // namespace

ModeSelector::ModeSelector(BootMode current_mode, Display* display,
                           std::function<void()> short_click,
                           std::function<void()> before_menu)
    : current_mode_(current_mode),
      selected_mode_(current_mode),
      display_(display),
      short_click_(std::move(short_click)),
      before_menu_(std::move(before_menu)) {
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
        if (pressed_) {
            return;
        }

        // Detect fast double-click (second press down within window of first press release)
        if (!menu_active_ && pending_single_click_) {
            const int64_t time_since_release_us = now - last_release_us_;
            if (time_since_release_us <= MsToUs(kDoubleClickWindowMs)) {
                pending_single_click_ = false;
                double_click = true;
                target_mode = (current_mode_ == BootMode::Xiaozhi) ? BootMode::Chronchi : BootMode::Xiaozhi;
            } else {
                pending_single_click_ = false;
            }
        }

        pressed_ = true;
        confirm_fired_ = false;
        press_started_us_ = now;
    }

    if (double_click) {
        ESP_LOGI(kTag, "Fast double-click on GPIO 3 detected! Switching mode from %s to %s",
                 BootModeName(current_mode_), BootModeName(target_mode));
        Confirm(target_mode);
    }
}

void ModeSelector::OnPressUp() {
    bool update_menu = false;
    BootMode selected = BootMode::Xiaozhi;
    const int64_t now = esp_timer_get_time();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!pressed_) {
            return;
        }
        pressed_ = false;
        const int64_t held_us = now - press_started_us_;

        if (menu_active_) {
            if (must_release_) {
                // The press that entered the menu can never confirm it.
                must_release_ = false;
                menu_deadline_us_ = now + MsToUs(kMenuTimeoutMs);
            } else if (!confirm_fired_ && held_us < MsToUs(kShortClickMs)) {
                // Cycle through 2 modes: Xiaozhi <-> Chronchi
                int current = static_cast<int>(selected_mode_);
                int next = (current + 1) % kModeCount;
                selected_mode_ = static_cast<BootMode>(next);
                menu_deadline_us_ = now + MsToUs(kMenuTimeoutMs);
                selected = selected_mode_;
                update_menu = true;
            }
        } else if (held_us < MsToUs(kShortClickMs)) {
            // Arm pending single-click. If a 2nd click arrives within kDoubleClickWindowMs,
            // OnPressDown triggers double-click mode switch. If window expires, Tick fires short_click_.
            pending_single_click_ = true;
            last_release_us_ = now;
        } else {
            pending_single_click_ = false;
        }
    }

    if (update_menu && display_ != nullptr) {
        display_->ShowModeMenu(static_cast<int>(selected));
    }
}

bool ModeSelector::IsMenuActive() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return menu_active_;
}

void ModeSelector::TimerCallback(void* arg) {
    static_cast<ModeSelector*>(arg)->Tick();
}

void ModeSelector::RestartCallback(void*) {
    esp_restart();
}

void ModeSelector::Tick() {
    bool enter_menu = false;
    bool cancel_menu = false;
    bool confirm = false;
    std::function<void()> fire_single_click;
    BootMode selected = BootMode::Xiaozhi;
    const int64_t now = esp_timer_get_time();

    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Fire pending single click if double-click window expired without a second press
        if (pending_single_click_ && !pressed_) {
            if (now - last_release_us_ >= MsToUs(kDoubleClickWindowMs)) {
                pending_single_click_ = false;
                fire_single_click = short_click_;
            }
        }

        if (!menu_active_ && pressed_ &&
            now - press_started_us_ >= MsToUs(kEnterMenuMs)) {
            pending_single_click_ = false;
            menu_active_ = true;
            must_release_ = true;
            selected_mode_ = current_mode_;
            menu_deadline_us_ = now + MsToUs(kMenuTimeoutMs);
            enter_menu = true;
            selected = selected_mode_;
        } else if (menu_active_ && !must_release_ && pressed_ && !confirm_fired_ &&
                   now - press_started_us_ >= MsToUs(kConfirmMs)) {
            confirm_fired_ = true;
            menu_active_ = false;
            selected = selected_mode_;
            confirm = true;
        } else if (menu_active_ && now >= menu_deadline_us_) {
            menu_active_ = false;
            must_release_ = false;
            cancel_menu = true;
        }
    }

    if (fire_single_click) {
        fire_single_click();
    }

    if (enter_menu) {
        ESP_LOGI(kTag, "Mode menu opened (current=%s)", BootModeName(current_mode_));
        if (before_menu_) {
            before_menu_();
        }
        if (display_ != nullptr) {
            display_->ShowModeMenu(static_cast<int>(selected));
        }
    } else if (cancel_menu) {
        ESP_LOGI(kTag, "Mode menu timed out");
        if (display_ != nullptr) {
            display_->HideModeMenu();
        }
    } else if (confirm) {
        Confirm(selected);
    }
}

void ModeSelector::Confirm(BootMode selected) {
    if (selected == current_mode_) {
        ESP_LOGI(kTag, "Mode unchanged: %s", BootModeName(selected));
        if (display_ != nullptr) {
            display_->HideModeMenu();
            display_->ShowNotification("Mode unchanged", 1500);
        }
        return;
    }

    esp_err_t err = ModeStore::Save(selected);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to save mode: %s", esp_err_to_name(err));
        if (display_ != nullptr) {
            display_->HideModeMenu();
            display_->ShowNotification("Mode save failed", 3000);
        }
        return;
    }

    if (display_ != nullptr) {
        display_->ShowModeSwitching(BootModeName(selected));
    }
    esp_timer_stop(restart_timer_);
    ESP_ERROR_CHECK(esp_timer_start_once(restart_timer_, 700 * 1000));
}
