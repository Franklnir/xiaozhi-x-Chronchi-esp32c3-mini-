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
    std::lock_guard<std::mutex> lock(mutex_);
    if (pressed_) {
        return;
    }
    pressed_ = true;
    confirm_fired_ = false;
    press_started_us_ = esp_timer_get_time();
}

void ModeSelector::OnPressUp() {
    std::function<void()> short_click;
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
                selected_mode_ = selected_mode_ == BootMode::Xiaozhi
                                     ? BootMode::Chronchi : BootMode::Xiaozhi;
                menu_deadline_us_ = now + MsToUs(kMenuTimeoutMs);
                selected = selected_mode_;
                update_menu = true;
            }
        } else if (held_us < MsToUs(kShortClickMs)) {
            short_click = short_click_;
        }
    }

    if (update_menu && display_ != nullptr) {
        display_->ShowModeMenu(selected == BootMode::Chronchi);
    }
    if (short_click) {
        short_click();
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
    BootMode selected = BootMode::Xiaozhi;
    const int64_t now = esp_timer_get_time();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!menu_active_ && pressed_ &&
            now - press_started_us_ >= MsToUs(kEnterMenuMs)) {
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

    if (enter_menu) {
        ESP_LOGI(kTag, "Mode menu opened (current=%s)", BootModeName(current_mode_));
        if (before_menu_) {
            before_menu_();
        }
        if (display_ != nullptr) {
            display_->ShowModeMenu(selected == BootMode::Chronchi);
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
    ESP_ERROR_CHECK(esp_timer_start_once(restart_timer_, 700 * 1000));
}
