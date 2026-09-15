# Xichi Mode: Voice Notification Reader via Firebase — FINAL DESIGN

**Version:** 4.0 — BLE provisioning in Xichi mode

---

## Flow Utama

```
User masuk Xichi mode
    │
    ├─ NVS ada config? ──Ya──► Langsung mulai Wi-Fi + polling
    │
    └─ Tidak ──► BLE aktif (mode="xichi")
                  │
                  ├─ App detect mode "xichi"
                  │   → Kirim FIREBASE_CONFIG
                  │   → ESP32 simpan ke NVS
                  │   → Matikan BLE
                  │   → Mulai Wi-Fi + polling
                  │
                  └─ Timeout 2 menit
                      → Matikan BLE
                      → Mulai Wi-Fi (tanpa config)
                      → Tampilkan "No API Secret"
```

## BLE Provisioning (Otomatis)

ESP32 di Xichi mode:
- BLE advertise sebagai "Chronchi-XXXX"
- DeviceStatus response mengandung `"mode": "xichi"`
- App otomatis deteksi mode dan kirim config

ESPBridge App:
- Saat BLE connect → baca DeviceStatus → cek `mode` field
- Jika `mode == "xichi"` → otomatis kirim FIREBASE_CONFIG
- Jika `mode == "chronchi"` → perilaku normal (tidak kirim config)

## State Machine Xichi

```
BleProvisioning → BleConfirmed → Standby → Polling → OpenChannel
     │                │                        ↓
     │ (timeout)       │              SendToServer → WaitingForTTS → Speaking
     │                │                                                │
     └────────────────┘                                                │
              ↓                                                         │
         Standby (no config) ←─────────────────────────────────────────┘
```

## RAM Budget

| Phase | Free Heap | Notes |
|-------|-----------|-------|
| BLE Provisioning | 200-240 KB | BLE only, no Wi-Fi |
| Standby (Wi-Fi) | 150-190 KB | BLE freed, Wi-Fi active |
| Speaking | 100-150 KB | +Opus decoder |

## File Changes

### ESP32
```
chronchi/chronchi_ble.h    + mode param, +Stop() method
chronchi/chronchi_ble.cc   + mode in DeviceStatus, +Stop() impl
xichi/xichi_mode.h         Rewrite: BLE provisioning phase
xichi/xichi_mode.cc        Rewrite: BLE→config→stop BLE→Wi-Fi
```

### ESPBridge App
```
BleConstants.kt            +FIREBASE_CONFIG, +CLEAR_CONFIG
BleConnectionManager.kt    +mode field in ConnectedDeviceInfo
TransportRouter.kt         +sendFirebaseConfig(), +sendClearConfig()
DeviceConnectionService.kt Auto-send config when mode=="xichi"
MainViewModel.kt           +setFirebaseSecret(), +clear on forget/logout
SettingsScreen.kt          +Firebase Database Secret input field
AppNavHost.kt              +onFirebaseSecret param
```
