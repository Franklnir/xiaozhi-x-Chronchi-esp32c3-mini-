#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "lvgl_display/lvgl_display.h"
#include "chronchi/chronchi_models.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

class OledDisplay : public LvglDisplay {
public:
    OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                int width, int height, bool mirror_x, bool mirror_y,
                bool animated_mode, bool chronchi_mode);
    ~OledDisplay();

    virtual void SetEmotion(const char* emotion) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void SetTheme(Theme* theme) override;
    virtual void SetFaceState(FaceState state) override;
    virtual void ShowModeMenu(int selected_mode) override;
    virtual void HideModeMenu() override;
    virtual void ShowModeSwitching(const char* mode_name) override;
    virtual void SetChronchiScreen(const ChronchiScreen& screen) override;

protected:
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

private:
    // Setup methods
    void SetupUI_128x64();
    void SetupUI_128x32();
    void SetupModeOverlay();
    void SetupChronchiOverlay();

    // Chronchi helpers
    void ResetChronchiLayout();
    void DrawChronchiBitmap(const uint8_t* bitmap, int bitmap_width, int bitmap_height);

    // Animated face methods
    void UpdateFace();
    void IdleBehavior(int base_eye_height);
    void ListeningBehavior(int base_eye_height);
    void SpeakingBehavior(int eye_height);

    // Constants
    static constexpr int kEyeSize = 14;

    // Hardware handles
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    // Mode flags
    bool animated_mode_ = false;
    bool chronchi_mode_ = false;

    // Layout containers
    lv_obj_t* container_ = nullptr;
    lv_obj_t* top_bar_ = nullptr;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* content_left_ = nullptr;
    lv_obj_t* content_right_ = nullptr;

    // Content labels
    lv_obj_t* emotion_label_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;

    // Mode overlay
    lv_obj_t* mode_overlay_ = nullptr;
    lv_obj_t* mode_selection_label_ = nullptr;
    lv_obj_t* mode_help_label_ = nullptr;

    // Chronchi overlay
    lv_obj_t* chronchi_overlay_ = nullptr;
    lv_obj_t* chronchi_ble_icon_ = nullptr;
    lv_obj_t* chronchi_header_left_ = nullptr;
    lv_obj_t* chronchi_primary_ = nullptr;
    lv_obj_t* chronchi_secondary_ = nullptr;
    lv_obj_t* chronchi_footer_ = nullptr;
    lv_obj_t* chronchi_icon_canvas_ = nullptr;
    uint8_t chronchi_icon_buffer_[32 * 32 / 8] = {};

    // Animated face objects
    lv_obj_t* face_container_ = nullptr;
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    lv_obj_t* mouth_ = nullptr;
    lv_timer_t* face_timer_ = nullptr;
    FaceState face_state_ = FaceState::Idle;

    // Animation state
    int blink_phase_ = 0;
    int idle_move_offset_x_ = 0;
    int idle_move_offset_y_ = 0;
    int speak_mouth_current_ = 3;
    int speak_mouth_target_ = 3;
    uint32_t speak_last_update_ = 0;

    bool chronchi_layout_initialized_ = false;
};

#endif // OLED_DISPLAY_H
