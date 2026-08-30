"""Static/release contract checks for the ESP32-C3 dual-mode firmware.

These checks complement, but do not replace, on-device BLE/audio/display tests.
"""

from __future__ import annotations

import csv
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def parse_int(value: str) -> int:
    return int(value.strip(), 0)


def check_partition_table() -> None:
    rows: list[tuple[str, int, int]] = []
    with (ROOT / "partitions/v2/4m.csv").open(encoding="utf-8") as stream:
        for raw in csv.reader(line for line in stream if not line.lstrip().startswith("#")):
            if not raw or not raw[0].strip():
                continue
            rows.append((raw[0].strip(), parse_int(raw[3]), parse_int(raw[4])))

    partition_names = {name for name, _, _ in rows}
    assert partition_names >= {"nvs", "assets"}
    assert "factory" in partition_names or {"rescue", "main"} <= partition_names
    ordered = sorted(rows, key=lambda item: item[1])
    for previous, current in zip(ordered, ordered[1:]):
        assert previous[1] + previous[2] <= current[1], (previous, current)
    assert max(offset + size for _, offset, size in rows) <= 4 * 1024 * 1024

    sizes = {name: size for name, _, size in rows}
    app = ROOT / "build/xiaozhi.bin"
    assets = ROOT / "build/generated_assets.bin"
    if app.exists():
        app_partition = "main" if "main" in sizes else "factory"
        assert app.stat().st_size < sizes[app_partition] * 0.95
    if assets.exists():
        assert assets.stat().st_size <= sizes["assets"]


def check_mode_contract() -> None:
    boot = read("main/mode/boot_mode.h")
    store = read("main/mode/mode_store.cc")
    selector = read("main/mode/mode_selector.h") + read("main/mode/mode_selector.cc")
    main = read("main/main.cc")

    assert "Xiaozhi = 0" in boot and "Chronchi = 1" in boot
    assert 'kNamespace[] = "system_mode"' in store
    assert 'kKey[] = "boot_mode"' in store
    assert "nvs_commit(handle)" in store
    assert "DefaultMode()" in store and "defaulting to %s" in store
    assert "kShortClickMs = 700" in selector
    assert "kEnterMenuMs = 2500" in selector
    assert "kConfirmMs = 2000" in selector
    assert "kMenuTimeoutMs = 10000" in selector
    assert "must_release_ = true" in selector
    assert "ModeStore::Save(selected)" in selector
    assert "esp_restart()" in selector
    assert "ChronchiMode chronchi" in main and "Application::GetInstance()" in main


def check_radio_and_display_contract() -> None:
    chronchi_mode = read("main/chronchi/chronchi_mode.cc")
    chronchi_files = "\n".join(
        path.read_text(encoding="utf-8")
        for path in (ROOT / "main/chronchi").glob("*.*")
    )
    board = read("main/boards/esp32c3-inmp441/esp32c3_inmp441_board.cc")
    shared = read("main/boards/esp32c3-inmp441/shared_oled.cc")
    oled = read("main/display/oled_display.cc")

    assert "wifi" not in "\n".join(
        line.lower() for line in chronchi_mode.splitlines() if line.startswith("#include")
    )
    assert "NimBLE-Arduino" not in chronchi_files
    assert "GetEsp32c3SharedOled(true)" in chronchi_mode
    assert "GetEsp32c3SharedOled(false)" in board
    assert "DISPLAY_SDA_PIN" in shared and "DISPLAY_SCL_PIN" in shared
    assert "0x3c, 0x3d" in shared
    assert "lv_font_montserrat_10" in oled
    assert "lv_font_montserrat_12" in oled
    assert "lv_font_montserrat_14" in oled
    assert "constexpr int kChronchiHeaderY = 0" in oled
    assert "constexpr int kChronchiHeaderHeight = 16" in oled
    assert "constexpr int kChronchiHeaderRightX = 68" in oled
    assert "constexpr int kChronchiBatteryX = 100" in oled
    assert "constexpr int kChronchiBatteryWidth = 28" in oled
    assert "chronchi_battery_" in oled
    assert "ConfigureChronchiBatteryLabel" in oled
    assert "chronchi_divider_" not in oled
    assert "chronchi_subdivider_" not in oled
    assert '"%.8s"' in oled and "char app_header[9]" in oled
    for row in ("kChronchiBodyRow1Y = 17", "kChronchiBodyRow2Y = 33",
                "kChronchiBodyRow3Y = 49", "kChronchiBodyRowHeight = 15"):
        assert row in oled
    assert '"%s S:%u N:%u"' not in oled
    assert '"%.7s"' not in oled
    assert "chronchi_icon_buffer_, 32, 32" in oled
    assert 'set_header("NAVIGASI", screen.time)' in oled
    assert 'set_header("NAVIGATION", screen.time)' not in oled
    reset_layout = oled.split("void OledDisplay::ResetChronchiLayout()", 1)[1].split(
        "void OledDisplay::DrawChronchiBitmap", 1
    )[0]
    assert "ConfigureChronchiHeaderPair(chronchi_header_left_, chronchi_header_right_)" in reset_layout
    assert '"  XIAOZHI\\n> CHRONCHI"' in oled
    assert "LV_LABEL_LONG_SCROLL_CIRCULAR" in oled
    assert "lv_color_black()" in oled and "lv_color_white()" in oled
    for asset in ("kDelivery32x32", "kPackage32x32", "kCancelled32x32",
                  "kStraight32x32", "kRoundabout32x32", "kArrive32x32"):
        assert asset in oled


def check_chronchi_contract() -> None:
    protocol = read("main/chronchi/chronchi_protocol.h") + read("main/chronchi/chronchi_protocol.cc")
    ble = read("main/chronchi/chronchi_ble.cc")
    state = read("main/chronchi/chronchi_state.h") + read("main/chronchi/chronchi_state.cc")
    models = read("main/chronchi/chronchi_models.h")

    assert "kMaximumJson = 768" in protocol
    assert "kMagic = 0x45" in protocol and "kVersion = 0x01" in protocol
    assert "kHeaderSize = 8" in protocol
    assert "FeedFrame" in protocol
    assert "cJSON_ParseWithLengthOpts" in protocol
    for packet in ("TimeSync = 0x01", "PhoneStatus = 0x02", "NetworkStatus = 0x03",
                   "Weather = 0x04", "Notification = 0x05", "Navigation = 0x06",
                   "Location = 0x07", "Ack = 0x0a", "SyncBegin = 0x0b", "SyncEnd = 0x0c"):
        assert packet in protocol
    for category in ("MESSAGE", "EMAIL", "PROFESSIONAL", "PAYMENT", "ORDER", "SYSTEM"):
        assert f'"{category}"' in protocol
    for field in ("sourceApp", "app", "primaryText", "secondaryText", "tertiaryText",
                  "paymentDirection", "orderStatus", "distanceText", "roadName",
                  "destinationDistanceText"):
        assert f'"{field}"' in protocol
    assert '"Chronchi-%02X%02X"' in ble and "BuildIdentity()" in ble
    assert "7c9e0001-6f2f-4d4d-9f25-0d7fd4f0a001" in ble
    assert "BLE_GATT_CHR_F_WRITE_NO_RSP" in ble
    assert "BLE_GATT_CHR_F_NOTIFY" in ble
    assert ble.count(".access_cb = ChronchiBle::GattAccess") >= 2
    assert "ble_gap_adv_start" in ble and "BLE_GAP_EVENT_DISCONNECT" in ble
    assert "ResponseKind::Ready" in ble and "ResponseKind::Ack" in ble
    assert "NotifyResponse" in ble and "response.sequence" in ble
    assert "connection_handle_.store(0xffff)" in ble
    for priority in ("Navigation: return 100", "Payment: return 80",
                     "Professional: return 70", "Order: return 60", "System: return 50"):
        assert priority in state
    for timeout in ("return 7000", "return 8000", "return 5000"):
        assert timeout in state
    for screen in ("Home", "Message", "Professional", "Payment", "Order", "Navigation", "System"):
        assert screen in models
    assert "navigation_active_" in state and "ActiveExpiredLocked" in state


def check_device_battery_contract() -> None:
    config = read("main/boards/esp32c3-inmp441/config.h")
    build_config = read("main/boards/esp32c3-inmp441/config.json")
    monitor = read("main/boards/common/adc_battery_monitor.h") + read(
        "main/boards/common/adc_battery_monitor.cc"
    )
    mode = read("main/chronchi/chronchi_mode.cc")
    state = read("main/chronchi/chronchi_state.cc")
    oled = read("main/display/oled_display.cc")

    assert "BATTERY_ADC_GPIO                    GPIO_NUM_1" in config
    assert "BATTERY_ADC_CHANNEL                 ADC_CHANNEL_1" in config
    assert "BATTERY_DIVIDER_UPPER_RESISTOR_OHM  100000.0f" in config
    assert "BATTERY_DIVIDER_LOWER_RESISTOR_OHM  100000.0f" in config
    assert "BATTERY_REFRESH_INTERVAL_MS         5000" in config
    assert '"CONFIG_OCV_SOC_MODEL_1=y"' in build_config
    assert '"CONFIG_BATTERY_STATE_SOFTWARE_ESTIMATION=y"' in build_config
    assert "bool ReadBatteryLevel(uint8_t& level)" in monitor
    assert "battery_monitor_->ReadBatteryLevel(level)" in mode
    assert "state_.UpdateDeviceBattery(level, battery_monitor_->IsCharging())" in mode
    assert "StampDeviceBatteryLocked" in state
    phone_status = state.split("void ChronchiState::UpdatePhoneStatus", 1)[1].split(
        "void ChronchiState::UpdateDeviceBattery", 1
    )[0]
    assert "home_.battery" not in phone_status
    assert 'lv_label_set_text(chronchi_battery_, buffer)' in oled
    assert 'std::snprintf(buffer, sizeof(buffer), "--%%")' in oled
    assert "IsChronchiNotificationScreen(screen.type)" in oled
    assert "ConfigureChronchiNotificationHeaderPair" in oled


def check_wake_word_contract() -> None:
    board_config = json.loads(read("main/boards/esp32c3-inmp441/config.json"))
    options = set(board_config["builds"][0]["sdkconfig_append"])
    for model in ("HIJASON", "HILEXIN", "HIESP", "NIHAOXIAOZHI"):
        assert f"CONFIG_SR_WN_WN9S_{model}=y" in options

    portal = read("components/78__esp-wifi-connect/assets/wifi_configuration.html")
    handler = read("components/78__esp-wifi-connect/wifi_configuration_ap.cc")
    wake = read("main/audio/wake_words/esp_wake_word.cc")
    for value in ("hijason", "hilexin", "hiesp", "nihaoxiaozhi"):
        assert f'value="{value}"' in portal
        assert f'"{value}"' in handler
    assert 'nvs_open("audio"' in handler and '"wake_word"' in handler
    assert 'Settings audio_settings("audio", false)' in wake
    assert 'GetString("wake_word", "hijason")' in wake


def main() -> None:
    checks = (
        check_partition_table,
        check_mode_contract,
        check_radio_and_display_contract,
        check_chronchi_contract,
        check_device_battery_contract,
        check_wake_word_contract,
    )
    for check in checks:
        check()
        print(f"PASS {check.__name__}")


if __name__ == "__main__":
    main()
