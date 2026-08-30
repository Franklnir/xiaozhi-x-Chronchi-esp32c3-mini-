import json
import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class ProductionContractTest(unittest.TestCase):
    def test_4mb_partition_table_has_permanent_recovery_and_main_slot(self):
        rows = {}
        for raw in (ROOT / "partitions/v2/4m.csv").read_text().splitlines():
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            fields = [field.strip() for field in line.split(",")]
            rows[fields[0]] = {
                "type": fields[1],
                "subtype": fields[2],
                "offset": int(fields[3], 16),
                "size": int(fields[4], 16),
            }

        self.assertEqual("factory", rows["rescue"]["subtype"])
        self.assertEqual(0x90000, rows["rescue"]["size"])
        self.assertEqual("ota_0", rows["main"]["subtype"])
        self.assertEqual(0x2A0000, rows["main"]["size"])
        self.assertEqual(0x400000, rows["assets"]["offset"] + rows["assets"]["size"])

    def test_production_variant_requires_4mb_recovery_security_and_rollback(self):
        config = json.loads(
            (ROOT / "main/boards/esp32c3-inmp441/config.json").read_text()
        )
        build = next(
            item for item in config["builds"]
            if item["name"] == "esp32c3-inmp441-production"
        )
        values = set(build["sdkconfig_append"])
        self.assertIn("CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y", values)
        self.assertIn("CONFIG_CHRONCHI_DEFAULT_MODE=y", values)
        self.assertIn("CONFIG_CHRONCHI_RECOVERY_OTA=y", values)
        self.assertIn("CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y", values)
        self.assertIn("CONFIG_BT_NIMBLE_SECURITY_ENABLE=y", values)
        self.assertIn("CONFIG_BT_NIMBLE_NVS_PERSIST=y", values)
        self.assertIn("CONFIG_BT_NIMBLE_SM_LEGACY=n", values)
        self.assertIn("CONFIG_BT_NIMBLE_SM_SC_ONLY=1", values)

    def test_firmware_enters_recovery_and_recovery_verifies_signed_ota(self):
        ota = (ROOT / "main/chronchi/chronchi_ota.cc").read_text()
        recovery = (ROOT / "recovery/main/recovery_main.c").read_text()
        self.assertIn("OtaEnterRecovery", ota)
        self.assertIn("esp_ota_set_boot_partition", ota)
        self.assertIn("verify_release_signature", recovery)
        self.assertIn("mbedtls_pk_verify", recovery)
        self.assertIn("MIN_VERSION_KEY", recovery)
        self.assertIn("enforce_anti_rollback", recovery)
        self.assertIn("firmware downgrade rejected", recovery)
        self.assertIn("does not match image version", recovery)
        self.assertIn("esp_ota_write", recovery)
        self.assertIn("firmware SHA-256 mismatch", recovery)


if __name__ == "__main__":
    unittest.main()
