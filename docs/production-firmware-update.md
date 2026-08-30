# ESPBridge 4 MB Production Firmware Update

## Hardware and flash contract

Retail units use the ESP32-C3 Mini with exactly 4 MB flash. The production
variant `esp32c3-inmp441-production` uses `partitions/v2/4m.csv`:

- `rescue` factory app: 576 KB permanent BLE recovery updater
- `main` / `ota_0`: 2688 KB Xiaozhi + Chronchi application
- `assets`: 768 KB, containing the four configured wake-word models
- NVS: shared by main and recovery so BLE bonds survive an update
- OTA metadata and PHY data remain in dedicated partitions

Sample music is intentionally excluded. It is not a product feature and would
reduce the space needed for recovery. The recovery partition is never updated
through BLE.

## Safe two-stage update flow

1. Android connects through an encrypted, bonded BLE link and reads device
   identity, board, update strategy, role, and maximum image size.
2. Android downloads schema-2 manifest metadata over HTTPS and verifies its
   ECDSA P-256 manifest signature.
3. Android checks the exact image size and SHA-256.
4. Main firmware persists the recovery-request flag, selects the factory
   `rescue` partition, acknowledges Android, and restarts.
5. Android reconnects to the same trusted BLE address. Recovery reports
   `role=recovery` before any erase is allowed.
6. Android sends compact binary metadata including board, version, size,
   SHA-256, and the release signature.
7. Recovery independently verifies the ECDSA P-256 release signature with its
   embedded public key and rejects semantic versions below the version floor
   persisted in NVS, then writes sequential chunks only to `main`.
8. Recovery verifies the complete SHA-256 and ESP-IDF image, confirms the
   embedded app version matches the signed metadata, advances the version
   floor, selects `main`, clears the recovery flag, and restarts.

If power or BLE is lost after step 4, the device remains in recovery. Opening
the app again retries the update; recovery itself is still intact. This layout
does not keep the previous full firmware image because two 2.6 MB images cannot
fit in 4 MB.

## First factory flash

The production package must flash these fixed offsets:

```text
0x0000  bootloader.bin
0x8000  partition-table.bin
0xd000  ota_data_initial.bin
0x10000 chronchi-recovery.bin
0xa0000 chronchi-main.bin
0x340000 chronchi-assets.bin
```

On the first boot, recovery sees a valid main image and no recovery request,
selects `main`, and restarts automatically. Holding GPIO3 low while booting
keeps the unit in recovery for service work.

## Release signing

The production ECDSA private key is outside the repository at:

```text
C:\Users\frank\.espbridge-secrets\ota-production\espbridge-ota-private.pem
```

Back it up to an offline secret manager or HSM. Losing it prevents future
updates; leaking it allows unauthorized firmware releases. Only the public key
is embedded in Android and recovery.

`Publish-Firmware.ps1` creates a schema-2 manifest with two signatures:

- `deviceSignature` authenticates board, version, size, and SHA-256 to recovery.
- `signature` authenticates the complete HTTPS manifest to Android.

```powershell
$env:Path = 'C:\Program Files\Git\usr\bin;' + $env:Path
.\tools\production\Publish-Firmware.ps1 `
  -Firmware .\build-production\xiaozhi.bin `
  -Version 2.2.0 `
  -Board esp32c3-inmp441-production `
  -DownloadUrl https://updates.example.com/chronchi/2.2.0/chronchi-main.bin `
  -PrivateKey C:\Users\frank\.espbridge-secrets\ota-production\espbridge-ota-private.pem `
  -OutputManifest .\release\manifest.json `
  -Notes 'Stability and notification improvements'
```

Configure `ESPBRIDGE_OTA_MANIFEST_URL` in the Android release build. The public
key already matches recovery; it may still be supplied explicitly through
`ESPBRIDGE_OTA_PUBLIC_KEY_B64` in protected CI.

## Production approval gates

Before sale, test interrupted transfer at multiple percentages, power loss
during erase/write/finalization, rejected signatures, wrong-board images,
maximum-size images, repeated reconnects, Android reboot, Bluetooth toggle,
Forget Device, and a staged update across a pilot batch. Secure Boot V2 and
Flash Encryption release-mode eFuse provisioning remain factory operations and
must not be burned until the tested signing/provisioning procedure is approved.
