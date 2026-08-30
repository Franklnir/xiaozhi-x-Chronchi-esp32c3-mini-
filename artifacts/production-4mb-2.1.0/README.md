# Chronchi / ESPBridge Production Firmware 2.1.0

Target: ESP32-C3 Mini with **4MB flash** and board profile `esp32c3-inmp441`.

## Factory programming

`chronchi-factory-4mb.bin` is a complete 4MB manufacturing image. It initializes the permanent recovery application, main Xiaozhi application, OTA metadata, and assets. Flash it only at offset `0x0`:

```powershell
python -m esptool --chip esp32c3 --port COM3 write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 chronchi-factory-4mb.bin
```

The complete factory image resets NVS, including Wi-Fi, Bluetooth bonds, and trusted settings. This is correct for a new unit. Do not use it as a normal field update.

## Service flash that preserves NVS

Use the individual images when servicing an existing unit and preserving NVS at `0x9000`:

```powershell
python -m esptool --chip esp32c3 --port COM3 write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB `
  0x0 bootloader.bin `
  0x8000 partition-table.bin `
  0xd000 ota_data_initial.bin `
  0x10000 chronchi-recovery.bin `
  0xa0000 chronchi-main.bin `
  0x340000 chronchi-assets.bin
```

The first boot enters recovery, validates the main application, selects it, and restarts. Holding GPIO3 low during reset keeps the unit in recovery for service.

## Android update policy

ESPBridge Android does not permit firmware updates for this ESP32-C3 Mini 4 MB
product. Service existing units through USB with the commands above. The
permanent recovery and signed-image validation remain in the firmware as a safe
service foundation, but the Android application will not enter or transfer to
recovery for this board.

The Firmware Update module remains in ESPBridge for a future supported product.
ESP32-S3 N16R8 support is not implemented or enabled in the current release.

`manifest.example.json` proves the signing pipeline but contains the intentionally unusable domain `example.invalid`. Do not publish it or configure an Android manifest endpoint for this C3 product.

Never use the older 8MB package on this hardware. Verify every file against `SHA256SUMS.txt` before manufacturing or publishing.
