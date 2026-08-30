#include "oled_display.h"
#include "assets/lang_config.h"
#include "lvgl_theme.h"
#include "lvgl_font.h"
#include "chronchi/chronchi_assets.h"
#include "chronchi/chronchi_models.h"

#include <string>
#include <algorithm>
#include <cstdlib>
#include <cstdio>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include <font_awesome.h>

#define TAG "OledDisplay"

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(BUILTIN_ICON_FONT);
LV_FONT_DECLARE(font_awesome_30_1);
LV_FONT_DECLARE(lv_font_montserrat_10);
LV_FONT_DECLARE(lv_font_montserrat_12);
LV_FONT_DECLARE(lv_font_montserrat_14);

namespace {
constexpr int kCompactLabelX = 2;
constexpr int kCompactLabelWidth = 124;
constexpr int kCompactRowHeight = 13;
constexpr int kChronchiHeaderY = 0;
constexpr int kChronchiHeaderHeight = 16;
constexpr int kChronchiHeaderLeftWidth = 68;
constexpr int kChronchiHeaderRightX = 68;
constexpr int kChronchiHeaderRightWidth = 32;
constexpr int kChronchiBatteryX = 100;
constexpr int kChronchiBatteryWidth = 28;
constexpr int kChronchiBodyX = 3;
constexpr int kChronchiBodyWidth = 122;
constexpr int kChronchiBodyRowHeight = 15;
constexpr int kChronchiBodyRow1Y = 17;
constexpr int kChronchiBodyRow2Y = 33;
constexpr int kChronchiBodyRow3Y = 49;

void ConfigureCompactLabel(lv_obj_t* label, int y, lv_label_long_mode_t long_mode) {
    lv_obj_set_pos(label, kCompactLabelX, y);
    lv_obj_set_size(label, kCompactLabelWidth, kCompactRowHeight);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_label_set_long_mode(label, long_mode);
}

void ConfigureChronchiLabel(lv_obj_t* label, int x, int y, int width, int height,
                            lv_text_align_t align, lv_label_long_mode_t long_mode) {
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, width, height);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_line_space(label, 0, 0);
    lv_label_set_long_mode(label, long_mode);
}

void ConfigureChronchiBodyLabel(lv_obj_t* label, int y, int height,
                                lv_text_align_t align, lv_label_long_mode_t long_mode,
                                int x = kChronchiBodyX, int width = kChronchiBodyWidth) {
    ConfigureChronchiLabel(label, x, y, width, height, align, long_mode);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
}

void ConfigureChronchiHeaderPair(lv_obj_t* left, lv_obj_t* right) {
    ConfigureChronchiLabel(left, 0, kChronchiHeaderY, kChronchiHeaderLeftWidth,
                           kChronchiHeaderHeight, LV_TEXT_ALIGN_LEFT, LV_LABEL_LONG_CLIP);
    ConfigureChronchiLabel(right, kChronchiHeaderRightX, kChronchiHeaderY,
                           kChronchiHeaderRightWidth, kChronchiHeaderHeight,
                           LV_TEXT_ALIGN_RIGHT, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(left, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_font(right, &lv_font_montserrat_12, 0);
}

void ConfigureChronchiNotificationHeaderPair(lv_obj_t* left, lv_obj_t* right) {
    ConfigureChronchiLabel(left, 0, kChronchiHeaderY, kChronchiHeaderLeftWidth,
                           kChronchiHeaderHeight, LV_TEXT_ALIGN_LEFT, LV_LABEL_LONG_CLIP);
    ConfigureChronchiLabel(right, kChronchiHeaderRightX, kChronchiHeaderY,
                           kChronchiHeaderRightWidth + kChronchiBatteryWidth,
                           kChronchiHeaderHeight, LV_TEXT_ALIGN_RIGHT, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(left, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_font(right, &lv_font_montserrat_12, 0);
}

void ConfigureChronchiBatteryLabel(lv_obj_t* battery) {
    ConfigureChronchiLabel(battery, kChronchiBatteryX, kChronchiHeaderY,
                           kChronchiBatteryWidth, kChronchiHeaderHeight,
                           LV_TEXT_ALIGN_RIGHT, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(battery, &lv_font_montserrat_10, 0);
}

void FormatChronchiAppHeader(const char* source_app, char* output, size_t output_size) {
    if (output_size == 0) return;
    std::snprintf(output, output_size, "%.8s", source_app ? source_app : "");
}

const char* OrderStatusLabel(ChronchiOrderStatus status) {
    switch (status) {
        case ChronchiOrderStatus::Confirmed: return "Pesanan dikonfirmasi";
        case ChronchiOrderStatus::Packed: return "Pesanan dikemas";
        case ChronchiOrderStatus::Shipped: return "Pesanan dikirim";
        case ChronchiOrderStatus::InTransit: return "Dalam perjalanan";
        case ChronchiOrderStatus::OutForDelivery: return "Sedang diantar";
        case ChronchiOrderStatus::Delivered: return "Pesanan diterima";
        case ChronchiOrderStatus::Cancelled: return "Pesanan dibatalkan";
        case ChronchiOrderStatus::Unknown: return "Status pesanan";
    }
    return "Status pesanan";
}

bool IsChronchiNotificationScreen(ChronchiScreenType type) {
    return type == ChronchiScreenType::Message ||
           type == ChronchiScreenType::Professional ||
           type == ChronchiScreenType::Payment ||
           type == ChronchiScreenType::Order ||
           type == ChronchiScreenType::System;
}
}  // namespace

OledDisplay::OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
    int width, int height, bool mirror_x, bool mirror_y, bool animated_mode, bool chronchi_mode)
    : panel_io_(panel_io), panel_(panel), animated_mode_(animated_mode),
      chronchi_mode_(chronchi_mode) {
    width_ = width;
    height_ = height;

    auto text_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_TEXT_FONT);
    auto icon_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_ICON_FONT);
    auto large_icon_font = std::make_shared<LvglBuiltInFont>(&font_awesome_30_1);
    
    auto dark_theme = new LvglTheme("dark");
    dark_theme->set_text_font(text_font);
    dark_theme->set_icon_font(icon_font);
    dark_theme->set_large_icon_font(large_icon_font);

    auto& theme_manager = LvglThemeManager::GetInstance();
    theme_manager.RegisterTheme("dark", dark_theme);
    current_theme_ = dark_theme;

    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.task_stack = 6144;
#if CONFIG_SOC_CPU_CORES_NUM > 1
    port_cfg.task_affinity = 1;
#endif
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding OLED display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * height_),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (height_ == 64) {
        SetupUI_128x64();
    } else {
        SetupUI_128x32();
    }

    SetupModeOverlay();
    if (chronchi_mode_ && height_ == 64) {
        SetupChronchiOverlay();
    }

    if (animated_mode_ && height_ == 64) {
        face_timer_ = lv_timer_create([](lv_timer_t* t) {
            auto* self = static_cast<OledDisplay*>(lv_timer_get_user_data(t));
            self->UpdateFace();
        }, 60, this);
    }
}

OledDisplay::~OledDisplay() {
    if (face_timer_ != nullptr) {
        lv_timer_del(face_timer_);
        face_timer_ = nullptr;
    }

    if (mode_overlay_ != nullptr) {
        lv_obj_del(mode_overlay_);
        mode_overlay_ = nullptr;
    }
    if (chronchi_overlay_ != nullptr) {
        lv_obj_del(chronchi_overlay_);
        chronchi_overlay_ = nullptr;
    }

    if (content_ != nullptr) {
        lv_obj_del(content_);
    }

    bool is_128x64_layout = (top_bar_ != nullptr);
    if (status_bar_ != nullptr && is_128x64_layout) {
        status_label_ = nullptr;
        notification_label_ = nullptr;
        lv_obj_del(status_bar_);
    }
    if (top_bar_ != nullptr) {
        network_label_ = nullptr;
        mute_label_ = nullptr;
        battery_label_ = nullptr;
        lv_obj_del(top_bar_);
    }
    if (side_bar_ != nullptr) {
        if (!is_128x64_layout) {
            status_label_ = nullptr;
            notification_label_ = nullptr;
            network_label_ = nullptr;
            mute_label_ = nullptr;
            battery_label_ = nullptr;
        }
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    lvgl_port_deinit();
}

void OledDisplay::SetupModeOverlay() {
    DisplayLockGuard lock(this);
    auto screen = lv_screen_active();
    mode_overlay_ = lv_obj_create(screen);
    lv_obj_set_size(mode_overlay_, width_, height_);
    lv_obj_align(mode_overlay_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(mode_overlay_, 0, 0);
    lv_obj_set_style_border_width(mode_overlay_, 0, 0);
    lv_obj_set_style_pad_all(mode_overlay_, 0, 0);
    lv_obj_set_style_bg_color(mode_overlay_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(mode_overlay_, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(mode_overlay_, LV_SCROLLBAR_MODE_OFF);

    auto* title = lv_label_create(mode_overlay_);
    lv_label_set_text(title, "SELECT MODE");
    ConfigureCompactLabel(title, 1, LV_LABEL_LONG_CLIP);

    mode_selection_label_ = lv_label_create(mode_overlay_);
    lv_obj_set_pos(mode_selection_label_, kCompactLabelX, 16);
    lv_obj_set_size(mode_selection_label_, kCompactLabelWidth, 27);
    lv_obj_set_style_text_font(mode_selection_label_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_align(mode_selection_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(mode_selection_label_, lv_color_black(), 0);
    lv_obj_set_style_text_line_space(mode_selection_label_, 0, 0);
    lv_label_set_long_mode(mode_selection_label_, LV_LABEL_LONG_CLIP);

    mode_help_label_ = lv_label_create(mode_overlay_);
    ConfigureCompactLabel(mode_help_label_, 50, LV_LABEL_LONG_CLIP);
    lv_label_set_text(mode_help_label_, "CLICK:NEXT  HOLD:OK");
    lv_obj_add_flag(mode_overlay_, LV_OBJ_FLAG_HIDDEN);
}

void OledDisplay::SetupChronchiOverlay() {
    DisplayLockGuard lock(this);
    if (top_bar_ != nullptr) lv_obj_add_flag(top_bar_, LV_OBJ_FLAG_HIDDEN);
    if (status_bar_ != nullptr) lv_obj_add_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
    if (content_ != nullptr) lv_obj_add_flag(content_, LV_OBJ_FLAG_HIDDEN);

    auto screen = lv_screen_active();
    chronchi_overlay_ = lv_obj_create(screen);
    lv_obj_set_size(chronchi_overlay_, width_, height_);
    lv_obj_align(chronchi_overlay_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(chronchi_overlay_, 0, 0);
    lv_obj_set_style_border_width(chronchi_overlay_, 0, 0);
    lv_obj_set_style_pad_all(chronchi_overlay_, 0, 0);
    lv_obj_set_style_bg_color(chronchi_overlay_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(chronchi_overlay_, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(chronchi_overlay_, LV_SCROLLBAR_MODE_OFF);

    chronchi_header_left_ = lv_label_create(chronchi_overlay_);
    chronchi_header_right_ = lv_label_create(chronchi_overlay_);
    chronchi_battery_ = lv_label_create(chronchi_overlay_);
    ConfigureChronchiHeaderPair(chronchi_header_left_, chronchi_header_right_);
    ConfigureChronchiBatteryLabel(chronchi_battery_);
    chronchi_primary_ = lv_label_create(chronchi_overlay_);
    ConfigureChronchiBodyLabel(chronchi_primary_, kChronchiBodyRow1Y,
                               kChronchiBodyRowHeight, LV_TEXT_ALIGN_CENTER,
                               LV_LABEL_LONG_SCROLL_CIRCULAR);
    chronchi_secondary_ = lv_label_create(chronchi_overlay_);
    ConfigureChronchiBodyLabel(chronchi_secondary_, kChronchiBodyRow2Y, 31,
                               LV_TEXT_ALIGN_CENTER, LV_LABEL_LONG_WRAP);
    chronchi_footer_ = lv_label_create(chronchi_overlay_);
    ConfigureChronchiBodyLabel(chronchi_footer_, kChronchiBodyRow3Y,
                               kChronchiBodyRowHeight, LV_TEXT_ALIGN_CENTER,
                               LV_LABEL_LONG_SCROLL_CIRCULAR);

    chronchi_icon_canvas_ = lv_canvas_create(chronchi_overlay_);
    lv_canvas_set_buffer(chronchi_icon_canvas_, chronchi_icon_buffer_, 32, 32,
                         LV_COLOR_FORMAT_I1);
    lv_canvas_set_palette(chronchi_icon_canvas_, 0,
                          lv_color_to_32(lv_color_black(), LV_OPA_COVER));
    lv_canvas_set_palette(chronchi_icon_canvas_, 1,
                          lv_color_to_32(lv_color_white(), LV_OPA_COVER));
    lv_canvas_fill_bg(chronchi_icon_canvas_, lv_color_black(), LV_OPA_COVER);
    ResetChronchiLayout();
}

void OledDisplay::ShowModeMenu(bool chronchi_selected) {
    DisplayLockGuard lock(this);
    if (mode_overlay_ == nullptr) return;
    lv_label_set_text(mode_selection_label_, chronchi_selected
                                               ? "  XIAOZHI\n> CHRONCHI"
                                               : "> XIAOZHI\n  CHRONCHI");
    lv_label_set_text(mode_help_label_, "CLICK:NEXT  HOLD:OK");
    lv_obj_remove_flag(mode_overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(mode_overlay_);
}

void OledDisplay::HideModeMenu() {
    DisplayLockGuard lock(this);
    if (mode_overlay_ != nullptr) lv_obj_add_flag(mode_overlay_, LV_OBJ_FLAG_HIDDEN);
}

void OledDisplay::ShowModeSwitching(const char* mode_name) {
    DisplayLockGuard lock(this);
    if (mode_overlay_ == nullptr) return;
    char text[48];
    snprintf(text, sizeof(text), "SWITCHING TO\n%s", mode_name);
    lv_label_set_text(mode_selection_label_, text);
    lv_label_set_text(mode_help_label_, "RESTARTING...");
    lv_obj_remove_flag(mode_overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(mode_overlay_);
}

void OledDisplay::ResetChronchiLayout() {
    lv_obj_t* objects[] = {chronchi_header_left_, chronchi_header_right_, chronchi_battery_,
                           chronchi_primary_, chronchi_secondary_, chronchi_footer_,
                           chronchi_icon_canvas_};
    for (lv_obj_t* object : objects) {
        if (object != nullptr) lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
    // Startup/system screens temporarily center this label. Always restore the
    // edge-aligned header before rendering the next normal Chronchi screen.
    ConfigureChronchiHeaderPair(chronchi_header_left_, chronchi_header_right_);
    ConfigureChronchiBatteryLabel(chronchi_battery_);
    ConfigureChronchiBodyLabel(chronchi_primary_, kChronchiBodyRow1Y,
                               kChronchiBodyRowHeight, LV_TEXT_ALIGN_CENTER,
                               LV_LABEL_LONG_SCROLL_CIRCULAR);
    ConfigureChronchiBodyLabel(chronchi_secondary_, kChronchiBodyRow2Y, 31,
                               LV_TEXT_ALIGN_CENTER, LV_LABEL_LONG_WRAP);
    ConfigureChronchiBodyLabel(chronchi_footer_, kChronchiBodyRow3Y,
                               kChronchiBodyRowHeight, LV_TEXT_ALIGN_CENTER,
                               LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(chronchi_header_left_, "");
    lv_label_set_text(chronchi_header_right_, "");
    lv_label_set_text(chronchi_battery_, "");
    lv_label_set_text(chronchi_primary_, "");
    lv_label_set_text(chronchi_secondary_, "");
    lv_label_set_text(chronchi_footer_, "");
}

void OledDisplay::DrawChronchiBitmap(const uint8_t* bitmap, int bitmap_width,
                                     int bitmap_height) {
    if (chronchi_icon_canvas_ == nullptr || bitmap == nullptr ||
        bitmap_width <= 0 || bitmap_height <= 0 || bitmap_width > 32 || bitmap_height > 32) {
        return;
    }
    lv_canvas_fill_bg(chronchi_icon_canvas_, lv_color_black(), LV_OPA_COVER);
    const int offset_x = (32 - bitmap_width) / 2;
    const int offset_y = (32 - bitmap_height) / 2;
    const int stride = (bitmap_width + 7) / 8;
    for (int y = 0; y < bitmap_height; ++y) {
        for (int x = 0; x < bitmap_width; ++x) {
            if ((bitmap[y * stride + x / 8] & (0x80 >> (x & 7))) != 0) {
                lv_canvas_set_px(chronchi_icon_canvas_, offset_x + x, offset_y + y,
                                 lv_color_white(), LV_OPA_COVER);
            }
        }
    }
    lv_obj_remove_flag(chronchi_icon_canvas_, LV_OBJ_FLAG_HIDDEN);
}

void OledDisplay::SetChronchiScreen(const ChronchiScreen& screen) {
    DisplayLockGuard lock(this);
    if (chronchi_overlay_ == nullptr) return;
    ResetChronchiLayout();

    auto show = [](lv_obj_t* object) { lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN); };
    auto set_header = [&](const char* left, const char* right) {
        lv_label_set_text(chronchi_header_left_, left ? left : "");
        lv_label_set_text(chronchi_header_right_, right ? right : "");
        show(chronchi_header_left_);
        show(chronchi_header_right_);
    };

    char buffer[80] = {};
    char app_header[9] = {};
    FormatChronchiAppHeader(screen.source_app, app_header, sizeof(app_header));
    if (screen.battery_valid) {
        std::snprintf(buffer, sizeof(buffer), "%u%%", static_cast<unsigned>(screen.battery));
    } else {
        std::snprintf(buffer, sizeof(buffer), "--%%");
    }
    lv_label_set_text(chronchi_battery_, buffer);
    if (!IsChronchiNotificationScreen(screen.type)) {
        show(chronchi_battery_);
    }

    switch (screen.type) {
        case ChronchiScreenType::Home:
            set_header(screen.network[0] ? screen.network : (screen.wifi ? "WIFI" : "BLE"), "");
            lv_label_set_text(chronchi_primary_, screen.time);
            lv_label_set_text(chronchi_secondary_, screen.date);
            ConfigureChronchiBodyLabel(chronchi_secondary_, kChronchiBodyRow2Y,
                                       kChronchiBodyRowHeight, LV_TEXT_ALIGN_CENTER,
                                       LV_LABEL_LONG_SCROLL_CIRCULAR);
            std::snprintf(buffer, sizeof(buffer), "%s %s", screen.weather, screen.location);
            lv_label_set_text(chronchi_footer_, buffer);
            show(chronchi_primary_);
            show(chronchi_secondary_);
            show(chronchi_footer_);
            break;

        case ChronchiScreenType::Message:
            ConfigureChronchiNotificationHeaderPair(chronchi_header_left_,
                                                     chronchi_header_right_);
            set_header(app_header, screen.time);
            lv_label_set_text(chronchi_primary_, screen.primary);
            lv_label_set_text(chronchi_secondary_, screen.secondary);
            ConfigureChronchiBodyLabel(chronchi_secondary_, kChronchiBodyRow2Y, 31,
                                       LV_TEXT_ALIGN_LEFT, LV_LABEL_LONG_WRAP);
            show(chronchi_primary_);
            show(chronchi_secondary_);
            break;

        case ChronchiScreenType::Professional:
        case ChronchiScreenType::Payment:
            ConfigureChronchiNotificationHeaderPair(chronchi_header_left_,
                                                     chronchi_header_right_);
            set_header(app_header, screen.time);
            lv_label_set_text(chronchi_primary_, screen.primary);
            lv_label_set_text(chronchi_secondary_, screen.secondary);
            ConfigureChronchiBodyLabel(chronchi_primary_, kChronchiBodyRow1Y,
                                       kChronchiBodyRowHeight, LV_TEXT_ALIGN_CENTER,
                                       LV_LABEL_LONG_SCROLL_CIRCULAR);
            ConfigureChronchiBodyLabel(chronchi_secondary_, kChronchiBodyRow2Y,
                                       kChronchiBodyRowHeight, LV_TEXT_ALIGN_CENTER,
                                       LV_LABEL_LONG_SCROLL_CIRCULAR);
            show(chronchi_primary_);
            show(chronchi_secondary_);
            break;

        case ChronchiScreenType::Order: {
            ConfigureChronchiNotificationHeaderPair(chronchi_header_left_,
                                                     chronchi_header_right_);
            set_header(app_header, screen.time);
            const uint8_t* bitmap = ChronchiAssets::kPackage32x32;
            if (screen.order_status == ChronchiOrderStatus::OutForDelivery) {
                bitmap = ChronchiAssets::kDelivery32x32;
            } else if (screen.order_status == ChronchiOrderStatus::Cancelled) {
                bitmap = ChronchiAssets::kCancelled32x32;
            }
            lv_obj_set_pos(chronchi_icon_canvas_, 44, 17);
            DrawChronchiBitmap(bitmap, 32, 32);
            lv_label_set_text(chronchi_footer_,
                              screen.primary[0] ? screen.primary : OrderStatusLabel(screen.order_status));
            show(chronchi_footer_);
            break;
        }

        case ChronchiScreenType::Navigation: {
            set_header("NAVIGASI", screen.time);
            const uint8_t* bitmap = ChronchiAssets::kStraight32x32;
            int icon_width = 32;
            int icon_height = 32;
            switch (screen.maneuver) {
                case ChronchiManeuver::Left:
                    bitmap = ChronchiAssets::kLeft21x15; icon_width = 21; icon_height = 15; break;
                case ChronchiManeuver::Right:
                    bitmap = ChronchiAssets::kRight28x20; icon_width = 28; icon_height = 20; break;
                case ChronchiManeuver::SlightLeft:
                    bitmap = ChronchiAssets::kSlightLeft15x24; icon_width = 15; icon_height = 24; break;
                case ChronchiManeuver::SlightRight:
                    bitmap = ChronchiAssets::kSlightRight15x24; icon_width = 15; icon_height = 24; break;
                case ChronchiManeuver::Roundabout:
                    bitmap = ChronchiAssets::kRoundabout32x32; break;
                case ChronchiManeuver::Arrive:
                    bitmap = ChronchiAssets::kArrive32x32; break;
                case ChronchiManeuver::Straight:
                case ChronchiManeuver::Unknown:
                    break;
            }
            lv_obj_set_pos(chronchi_icon_canvas_, 1, 17);
            DrawChronchiBitmap(bitmap, icon_width, icon_height);
            ConfigureChronchiBodyLabel(chronchi_primary_, kChronchiBodyRow1Y,
                                       kChronchiBodyRowHeight, LV_TEXT_ALIGN_LEFT,
                                       LV_LABEL_LONG_SCROLL_CIRCULAR, 43, 83);
            ConfigureChronchiBodyLabel(chronchi_secondary_, kChronchiBodyRow2Y,
                                       kChronchiBodyRowHeight, LV_TEXT_ALIGN_LEFT,
                                       LV_LABEL_LONG_SCROLL_CIRCULAR, 43, 83);
            ConfigureChronchiBodyLabel(chronchi_footer_, kChronchiBodyRow3Y,
                                       kChronchiBodyRowHeight, LV_TEXT_ALIGN_LEFT,
                                       LV_LABEL_LONG_SCROLL_CIRCULAR, 43, 83);
            lv_label_set_text(chronchi_primary_, screen.primary);
            lv_label_set_text(chronchi_secondary_, screen.secondary);
            lv_label_set_text(chronchi_footer_, screen.footer);
            show(chronchi_primary_);
            show(chronchi_secondary_);
            show(chronchi_footer_);
            break;
        }

        case ChronchiScreenType::Startup:
        case ChronchiScreenType::Connection:
        case ChronchiScreenType::System:
            ConfigureChronchiLabel(chronchi_header_left_, 0, 0,
                                    screen.type == ChronchiScreenType::System
                                        ? width_ : kChronchiBatteryX,
                                    16,
                                    LV_TEXT_ALIGN_CENTER, LV_LABEL_LONG_CLIP);
            lv_obj_set_style_text_font(chronchi_header_left_, &lv_font_montserrat_14, 0);
            lv_label_set_text(chronchi_header_left_, screen.source_app);
            ConfigureChronchiBodyLabel(chronchi_primary_, kChronchiBodyRow1Y,
                                       kChronchiBodyRowHeight, LV_TEXT_ALIGN_CENTER,
                                       LV_LABEL_LONG_SCROLL_CIRCULAR);
            ConfigureChronchiBodyLabel(chronchi_secondary_, kChronchiBodyRow2Y,
                                       kChronchiBodyRowHeight, LV_TEXT_ALIGN_CENTER,
                                       LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_label_set_text(chronchi_primary_, screen.primary);
            lv_label_set_text(chronchi_secondary_, screen.secondary);
            show(chronchi_header_left_);
            show(chronchi_primary_);
            show(chronchi_secondary_);
            break;
    }

    lv_obj_move_foreground(chronchi_overlay_);
    if (mode_overlay_ != nullptr && !lv_obj_has_flag(mode_overlay_, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_move_foreground(mode_overlay_);
    }
}

bool OledDisplay::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void OledDisplay::Unlock() {
    lvgl_port_unlock();
}

void OledDisplay::SetChatMessage(const char* role, const char* content) {
    (void)role;
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }

    // Replace all newlines with spaces
    std::string content_str = content;
    std::replace(content_str.begin(), content_str.end(), '\n', ' ');

    if (content_right_ == nullptr) {
        lv_label_set_text(chat_message_label_, content_str.c_str());
    } else {
        if (content == nullptr || content[0] == '\0') {
            lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(chat_message_label_, content_str.c_str());
            lv_obj_remove_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void OledDisplay::SetupUI_128x64() {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);

    /* Layer 1: Top bar - for status icons */
    top_bar_ = lv_obj_create(container_);
    lv_obj_set_size(top_bar_, LV_HOR_RES, 16);
    lv_obj_set_style_radius(top_bar_, 0, 0);
    lv_obj_set_style_bg_opa(top_bar_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_bar_, 0, 0);
    lv_obj_set_style_pad_all(top_bar_, 0, 0);
    lv_obj_set_flex_flow(top_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(top_bar_, LV_SCROLLBAR_MODE_OFF);

    network_label_ = lv_label_create(top_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);

    lv_obj_t* right_icons = lv_obj_create(top_bar_);
    lv_obj_set_size(right_icons, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(right_icons, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_icons, 0, 0);
    lv_obj_set_style_pad_all(right_icons, 0, 0);
    lv_obj_set_flex_flow(right_icons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right_icons, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    mute_label_ = lv_label_create(right_icons);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);

    battery_label_ = lv_label_create(right_icons);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);

    /* Layer 2: Status bar - for center text labels */
    status_bar_ = lv_obj_create(screen);
    lv_obj_set_size(status_bar_, LV_HOR_RES, 16);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);  // Transparent background
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_scrollbar_mode(status_bar_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_layout(status_bar_, LV_LAYOUT_NONE, 0);  // Use absolute positioning
    lv_obj_align(status_bar_, LV_ALIGN_TOP_MID, 0, 0);  // Overlap with top_bar_

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_width(notification_label_, LV_HOR_RES);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_align(notification_label_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_width(status_label_, LV_HOR_RES);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    lv_obj_align(status_label_, LV_ALIGN_CENTER, 0, 0);

    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);

    if (animated_mode_) {
        lv_obj_set_size(content_, 128, 48);
        lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        face_container_ = lv_obj_create(content_);
        lv_obj_set_size(face_container_, 128, 48);
        lv_obj_set_style_border_width(face_container_, 0, 0);
        lv_obj_set_style_bg_opa(face_container_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(face_container_, 0, 0);

        left_eye_ = lv_obj_create(face_container_);
        right_eye_ = lv_obj_create(face_container_);
        mouth_ = lv_obj_create(face_container_);

        lv_obj_set_style_bg_color(left_eye_, lv_color_black(), 0);
        lv_obj_set_style_bg_color(right_eye_, lv_color_black(), 0);
        lv_obj_set_style_bg_color(mouth_, lv_color_black(), 0);

        lv_obj_set_style_border_width(left_eye_, 0, 0);
        lv_obj_set_style_border_width(right_eye_, 0, 0);
        lv_obj_set_style_border_width(mouth_, 0, 0);

        lv_obj_set_style_radius(left_eye_, 2, 0);
        lv_obj_set_style_radius(right_eye_, 2, 0);
        lv_obj_set_style_radius(mouth_, 4, 0);

        IdleBehavior(kEyeSize);
    } else {
        lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_flex_main_place(content_, LV_FLEX_ALIGN_CENTER, 0);

        content_left_ = lv_obj_create(content_);
        lv_obj_set_size(content_left_, 32, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(content_left_, 0, 0);
        lv_obj_set_style_border_width(content_left_, 0, 0);

        emotion_label_ = lv_label_create(content_left_);
        lv_obj_set_style_text_font(emotion_label_, large_icon_font, 0);
        lv_label_set_text(emotion_label_, FONT_AWESOME_MICROCHIP_AI);
        lv_obj_center(emotion_label_);
        lv_obj_set_style_pad_top(emotion_label_, 8, 0);

        content_right_ = lv_obj_create(content_);
        lv_obj_set_size(content_right_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(content_right_, 0, 0);
        lv_obj_set_style_border_width(content_right_, 0, 0);
        lv_obj_set_flex_grow(content_right_, 1);
        lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);

        chat_message_label_ = lv_label_create(content_right_);
        lv_label_set_text(chat_message_label_, "");
        lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_width(chat_message_label_, width_ - 32);
        lv_obj_set_style_pad_top(chat_message_label_, 14, 0);

        // Start scrolling subtitle after a delay
        static lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_delay(&a, 1000);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
        lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);
    }

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, lv_color_black(), 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
}

void OledDisplay::SetupUI_128x32() {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_column(container_, 0, 0);

    /* Emotion label on the left side */
    content_ = lv_obj_create(container_);
    lv_obj_set_size(content_, 32, 32);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_style_radius(content_, 0, 0);

    emotion_label_ = lv_label_create(content_);
    lv_obj_set_style_text_font(emotion_label_, large_icon_font, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_MICROCHIP_AI);
    lv_obj_center(emotion_label_);

    /* Right side */
    side_bar_ = lv_obj_create(container_);
    lv_obj_set_size(side_bar_, width_ - 32, 32);
    lv_obj_set_flex_flow(side_bar_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(side_bar_, 0, 0);
    lv_obj_set_style_border_width(side_bar_, 0, 0);
    lv_obj_set_style_radius(side_bar_, 0, 0);
    lv_obj_set_style_pad_row(side_bar_, 0, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(side_bar_);
    lv_obj_set_size(status_bar_, width_ - 32, 16);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_obj_set_style_pad_left(status_label_, 2, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_pad_left(notification_label_, 2, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);

    chat_message_label_ = lv_label_create(side_bar_);
    lv_obj_set_size(chat_message_label_, width_ - 32, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(chat_message_label_, 2, 0);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(chat_message_label_, "");

    // Start scrolling subtitle after a delay
    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_delay(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);
}

void OledDisplay::SetEmotion(const char* emotion) {
    if (animated_mode_) {
        return;
    }

    const char* utf8 = font_awesome_get_utf8(emotion);
    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }
    if (utf8 != nullptr) {
        lv_label_set_text(emotion_label_, utf8);
    } else {
        lv_label_set_text(emotion_label_, FONT_AWESOME_NEUTRAL);
    }
}

void OledDisplay::SetTheme(Theme* theme) {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(theme);
    auto text_font = lvgl_theme->text_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
}

void OledDisplay::SetFaceState(FaceState state) {
    face_state_ = state;
}

void OledDisplay::IdleBehavior(int base_eye_height) {
    if (left_eye_ == nullptr || right_eye_ == nullptr || mouth_ == nullptr) {
        return;
    }

    if (std::rand() % 40 == 0) {
        idle_move_offset_x_ = (std::rand() % 5) - 2;
        idle_move_offset_y_ = (std::rand() % 3) - 1;
    }

    int eye_h = base_eye_height;
    int eye_w = static_cast<int>(eye_h * 0.65f);

    lv_obj_set_size(left_eye_, eye_w, eye_h);
    lv_obj_set_size(right_eye_, eye_w, eye_h);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -12 + idle_move_offset_x_, -5 + idle_move_offset_y_);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, 12 + idle_move_offset_x_, -5 + idle_move_offset_y_);

    lv_obj_set_size(mouth_, 14, 3);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, idle_move_offset_x_, 10 + idle_move_offset_y_);
}

void OledDisplay::ListeningBehavior(int base_eye_height) {
    if (left_eye_ == nullptr || right_eye_ == nullptr || mouth_ == nullptr) {
        return;
    }

    int right_h = base_eye_height;
    int right_w = static_cast<int>(right_h * 0.65f);
    int left_h = base_eye_height - 2;
    int left_w = static_cast<int>(left_h * 0.65f);

    lv_obj_set_size(left_eye_, left_w, left_h);
    lv_obj_set_size(right_eye_, right_w, right_h);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -16, -5);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, 4, -5);

    lv_obj_set_size(mouth_, 10, 2);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, -4, 10);
}

void OledDisplay::SpeakingBehavior(int eye_height) {
    if (left_eye_ == nullptr || right_eye_ == nullptr || mouth_ == nullptr) {
        return;
    }

    uint32_t now = lv_tick_get();

    lv_obj_set_size(left_eye_, static_cast<int>(eye_height * 0.65f), eye_height);
    lv_obj_set_size(right_eye_, static_cast<int>(eye_height * 0.65f), eye_height);
    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -30, -6);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, 30, -6);

    if (now - speak_last_update_ > static_cast<uint32_t>(90 + (std::rand() % 60))) {
        speak_last_update_ = now;
        int r = std::rand() % 100;
        if (r < 20) {
            speak_mouth_target_ = 2;
        } else if (r < 50) {
            speak_mouth_target_ = 6;
        } else if (r < 80) {
            speak_mouth_target_ = 10;
        } else {
            speak_mouth_target_ = 16;
        }
    }

    if (speak_mouth_current_ < speak_mouth_target_) {
        speak_mouth_current_ += 2;
    } else if (speak_mouth_current_ > speak_mouth_target_) {
        speak_mouth_current_ -= 2;
    }

    lv_obj_set_size(mouth_, 15, speak_mouth_current_);
    lv_obj_set_style_radius(mouth_, 8, 0);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, 18);
}

void OledDisplay::UpdateFace() {
    if (!animated_mode_ || height_ != 64 || face_container_ == nullptr) {
        return;
    }

    DisplayLockGuard lock(this);

    int eye_height = kEyeSize;
    if (blink_phase_ == 0) {
        if (std::rand() % 120 == 0) {
            blink_phase_ = 1;
        }
    }

    switch (blink_phase_) {
        case 1:
            eye_height = 4;
            blink_phase_ = 2;
            break;
        case 2:
            eye_height = 1;
            blink_phase_ = 3;
            break;
        case 3:
            eye_height = 6;
            blink_phase_ = 0;
            break;
        default:
            eye_height = kEyeSize;
            break;
    }

    switch (face_state_) {
        case FaceState::Idle:
            IdleBehavior(eye_height);
            break;
        case FaceState::Listening:
            ListeningBehavior(eye_height);
            break;
        case FaceState::Speaking:
            SpeakingBehavior(eye_height);
            break;
    }
}
