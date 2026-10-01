#ifndef XIAOZHI_CHRONCHI_ALERT_H_
#define XIAOZHI_CHRONCHI_ALERT_H_

#include <driver/gpio.h>
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <cstdint>

class ChronchiAlert {
public:
    static ChronchiAlert& GetInstance();

    void Initialize(gpio_num_t vibration_pin = GPIO_NUM_NC);

    // Getar untuk notifikasi umum (WhatsApp, Telegram, dll)
    void VibrateNotification();

    // Pola getar kustom
    void VibratePattern(uint32_t on_ms, uint32_t off_ms, uint8_t repeat);

    // Alarm pencarian perangkat dari Chronos (Find My Device)
    // Berdering nyaring maksimal seperti bell + getar terus menerus
    void StartFindDeviceAlarm();

    // Alarm panggilan telepon masuk
    void StartCallAlert();

    // Hentikan alarm / dering / getar (dapat dipicu tombol GPIO3 atau dismiss)
    void StopAlert();

    // Cek apakah alarm pencarian sedang berdering
    bool IsAlarmActive() const { return alarm_active_.load(); }

private:
    ChronchiAlert();
    ~ChronchiAlert();

    void EnsureAudioInitialized();
    void SetVibration(bool enable);
    static void AlertTask(void* param);

    gpio_num_t vibration_pin_ = GPIO_NUM_NC;
    i2s_chan_handle_t tx_handle_ = nullptr;
    TaskHandle_t alert_task_handle_ = nullptr;
    std::atomic<bool> alarm_active_{false};
    std::atomic<bool> audio_initialized_{false};
};

#endif  // XIAOZHI_CHRONCHI_ALERT_H_
