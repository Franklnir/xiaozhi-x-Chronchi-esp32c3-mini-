import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class ChronosProtocolTest(unittest.TestCase):
    def test_chronos_protocol_files_exist(self):
        proto_h = ROOT / "main/chronchi/chronos_protocol.h"
        proto_cc = ROOT / "main/chronchi/chronos_protocol.cc"
        self.assertTrue(proto_h.exists())
        self.assertTrue(proto_cc.exists())

    def test_chronos_service_and_characteristics_in_ble(self):
        ble_cc = (ROOT / "main/chronchi/chronchi_ble.cc").read_text(encoding="utf-8")
        # Dual GATT Service check:
        # ESPBridge Service UUID: 7c9e0001-6f2f-4d4d-9f25-0d7fd4f0a001
        self.assertIn("7c9e0001-6f2f-4d4d-9f25-0d7fd4f0a001", ble_cc)
        # Chronos NUS Service UUID: 6e400001-b5a3-f393-e0a9-e50e24dcca9e
        self.assertIn("6e400001-b5a3-f393-e0a9-e50e24dcca9e", ble_cc)
        self.assertIn("kChronosRxUuid", ble_cc)
        self.assertIn("kChronosTxUuid", ble_cc)
        self.assertIn("g_chronos_tx_value_handle", ble_cc)
        self.assertIn("kChronosCharacteristics", ble_cc)
        self.assertIn("NotifyChronos", ble_cc)
        self.assertIn("SendChronosHandshake", ble_cc)

        # Both services present in kServices
        services_def = ble_cc.split("static const ble_gatt_svc_def kServices[]", 1)[1].split("};", 1)[0]
        self.assertIn("kServiceUuid", services_def)
        self.assertIn("kChronosServiceUuid", services_def)

    def test_chronos_protocol_parser_capabilities(self):
        proto_cc = (ROOT / "main/chronchi/chronos_protocol.cc").read_text(encoding="utf-8")
        # Magic bytes 0xAB and 0xEA
        self.assertIn("0xAB", proto_cc)
        self.assertIn("0xEA", proto_cc)
        # Commands: TimeSync (0x93), Phone Battery (0x91), Weather (0x7E), Notification (0x72), Navigation (0xEF)
        self.assertIn("case 0x93:", proto_cc)
        self.assertIn("case 0x91:", proto_cc)
        self.assertIn("case 0x7E:", proto_cc)
        self.assertIn("case 0x72:", proto_cc)
        self.assertIn("case 0xEF:", proto_cc)
        self.assertIn("case 0x71:", proto_cc)  # Find Watch
        self.assertIn("case 0x9D:", proto_cc)  # Music

        # Handshake builders
        self.assertIn("BuildInfoPacket", proto_cc)
        self.assertIn("BuildBatteryPacket", proto_cc)
        self.assertIn("BuildEspPacket", proto_cc)
        self.assertIn("BuildNotifyBatteryPacket", proto_cc)
        self.assertIn("BuildSyncRequestPacket", proto_cc)
        self.assertIn("BuildFindPhonePacket", proto_cc)
        self.assertIn("BuildCameraCapturePacket", proto_cc)
        self.assertIn("BuildMusicControlPacket", proto_cc)

        # Chronos app commands
        self.assertIn("case 0xCA:", proto_cc)  # Watch Specs
        self.assertIn("case 0x79:", proto_cc)  # Camera ready
        self.assertIn("case 0x7C:", proto_cc)  # 24-hour mode
        self.assertIn("case 0x88:", proto_cc)  # High/Low temp

        # Maneuver recognition
        self.assertIn("ParseManeuverFromText", proto_cc)
        self.assertIn("ROUNDABOUT", proto_cc)
        self.assertIn("BUNDARAN", proto_cc)
        self.assertIn("KIRI", proto_cc)
        self.assertIn("KANAN", proto_cc)

        # App notification icons
        for app in ("WhatsApp", "Telegram", "Instagram", "Gmail", "SMS", "Panggilan"):
            self.assertIn(app, proto_cc)

    def test_chronos_protocol_robustness(self):
        proto_cc = (ROOT / "main/chronchi/chronos_protocol.cc").read_text(encoding="utf-8")
        # Navigation mode bit check for icons and flags
        self.assertIn("(mode & 0x80) != 0", proto_cc)
        # Notification dismiss handling
        self.assertIn("icon == 0x02 || state == 0x03", proto_cc)

    def test_dual_mode_cmakelists_includes_both_protocols(self):
        cmake = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("chronchi/chronchi_protocol.cc", cmake)
        self.assertIn("chronchi/chronos_protocol.cc", cmake)


    def test_chronchi_alert_and_vibration_integration(self):
        alert_h = ROOT / "main/chronchi/chronchi_alert.h"
        alert_cc = ROOT / "main/chronchi/chronchi_alert.cc"
        self.assertTrue(alert_h.exists())
        self.assertTrue(alert_cc.exists())

        cmake = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("chronchi/chronchi_alert.cc", cmake)

        chronos_cc = (ROOT / "main/chronchi/chronos_protocol.cc").read_text(encoding="utf-8")
        self.assertIn("StartFindDeviceAlarm", chronos_cc)
        self.assertIn("VibrateNotification", chronos_cc)

        config_h = (ROOT / "main/boards/esp32c3-inmp441/config.h").read_text(encoding="utf-8")
        self.assertIn("VIBRATION_MOTOR_PIN", config_h)


if __name__ == "__main__":
    unittest.main()
