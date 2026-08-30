#ifndef XIAOZHI_CHRONOS_MODE_H_
#define XIAOZHI_CHRONOS_MODE_H_

#include "chronos_ble.h"
#include "chronos_state.h"
#include "mode/mode_selector.h"
#include "boards/common/button.h"

#include <atomic>
#include <memory>

class Display;

class ChronosMode {
public:
    ChronosMode();
    void Initialize();
    [[noreturn]] void Run();

private:
    void NextPage();
    void Render(const ChronosSnapshot& snapshot);
    static const char* WeatherName(uint8_t icon);

    ChronosState state_;
    ChronosBle ble_;
    Button button_;
    std::unique_ptr<ModeSelector> mode_selector_;
    Display* display_ = nullptr;
    std::atomic<uint8_t> page_{0};
    esp_err_t ble_status_ = ESP_FAIL;
};

#endif  // XIAOZHI_CHRONOS_MODE_H_
