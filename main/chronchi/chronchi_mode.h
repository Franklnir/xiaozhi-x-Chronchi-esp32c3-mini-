#ifndef XIAOZHI_CHRONCHI_MODE_H_
#define XIAOZHI_CHRONCHI_MODE_H_

#include "chronchi_ble.h"
#include "chronchi_state.h"
#include "mode/mode_selector.h"
#include "boards/common/adc_battery_monitor.h"
#include "boards/common/button.h"

#include <memory>
#include <cstdint>

class Display;

class ChronchiMode {
public:
    ChronchiMode();
    void Initialize();
    [[noreturn]] void Run();

private:
    void Render(const ChronchiSnapshot& snapshot);

    ChronchiState state_;
    ChronchiBle ble_;
    Button button_;
    std::unique_ptr<ModeSelector> mode_selector_;
    Display* display_ = nullptr;
    esp_err_t ble_status_ = ESP_FAIL;
    int64_t rollback_confirmation_due_us_ = 0;

    // Triple-click GPIO3 to clear WiFi
    int click_count_ = 0;
    int64_t last_click_us_ = 0;
    static constexpr int64_t kTripleClickWindowUs = 1000000; // 1 second window
};

#endif  // XIAOZHI_CHRONCHI_MODE_H_
