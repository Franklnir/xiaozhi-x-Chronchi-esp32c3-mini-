# Feature: BLE Communication

---

## Overview

ESP32-C3 menggunakan NimBLE stack untuk BLE (Bluetooth Low Energy). Di Chronchi dan Xichi mode, BLE berfungsi sebagai jembatan komunikasi dengan HP Android via ESPBridge app.

---

## NimBLE Configuration

```c
// Dari sdkconfig
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1        // Hanya 1 HP
CONFIG_BT_NIMBLE_ROLE_PERIPHERAL=y        // ESP32 = peripheral
CONFIG_BT_NIMBLE_ROLE_BROADCASTER=y       // Bisa advertise
CONFIG_BT_NIMBLE_ROLE_CENTRAL=n           // Tidak scan device lain
CONFIG_BT_NIMBLE_ROLE_OBSERVER=n          // Tidak listen broadcast
CONFIG_BT_NIMBLE_GATT_SERVER=y            // ESP32 = GATT server
CONFIG_BT_NIMBLE_SECURITY_ENABLE=y        // Pairing terenkripsi
CONFIG_BT_NIMBLE_SM_SC=y                  // Secure Connections
CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU=247    // Max payload per packet
CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE=4096 // Task stack
CONFIG_BT_NIMBLE_PINNED_TO_CORE=0         // BLE di core 0
```

---

## GATT Service

### UUIDs

| Entity | UUID | Description |
|--------|------|-------------|
| Service | `7c9e0001-6f2f-4d4d-9f25-0d7fd4f0a001` | ESPBridge V1 service |
| RX Char | `7c9e0002-6f2f-4d4d-9f25-0d7fd4f0a001` | Phone → ESP32 (write) |
| TX Char | `7c9e0003-6f2f-4d4d-9f25-0d7fd4f0a001` | ESP32 → Phone (notify) |

### Characteristic Properties

| Char | Write | Notify | Encrypted |
|------|-------|--------|-----------|
| RX | ✅ (Write + WriteNoRsp) | ❌ | ✅ (jika security enabled) |
| TX | ❌ | ✅ | ❌ |

---

## ESPBridge V1 Protocol

### Frame Format

```
Byte 0:    Magic (0x45)
Byte 1:    Version (0x01)
Byte 2:    Packet type
Byte 3:    Sequence number
Byte 4:    Fragment index
Byte 5:    Fragment count
Byte 6-7:  Payload length (big-endian)
Byte 8+:   Payload (JSON)
```

### Fragmentation

Payload > MTU → dipecah jadi beberapa fragment:
- Fragment index: 0, 1, 2, ...
- Fragment count: total jumlah fragment
- Sequence: nomor urut pesan (wrap around 0-255)

**Max JSON:** 768 byte (termasuk semua fragment)

### Packet Types

| Type | Value | Direction | Description |
|------|-------|-----------|-------------|
| TimeSync | 0x01 | Phone→ESP | Sinkronisasi jam |
| PhoneStatus | 0x02 | Phone→ESP | Baterai HP |
| NetworkStatus | 0x03 | Phone→ESP | Status jaringan |
| Weather | 0x04 | Phone→ESP | Data cuaca |
| Notification | 0x05 | Phone→ESP | Notifikasi masuk |
| Navigation | 0x06 | Phone→ESP | Info navigasi |
| Location | 0x07 | Phone→ESP | Koordinat GPS |
| Command | 0x08 | Phone→ESP | Command ke ESP32 |
| DeviceStatus | 0x09 | Both | Capability request/response |
| Ack | 0x0A | ESP→Phone | Acknowledgement |
| SyncBegin | 0x0B | Phone→ESP | Mulai sync batch |
| SyncEnd | 0x0C | Phone→ESP | Selesai sync batch |
| OtaBegin | 0x0D | Phone→ESP | Mulai OTA |
| OtaChunk | 0x0E | Phone→ESP | Data OTA |
| OtaEnd | 0x0F | Phone→ESP | Selesai OTA |
| OtaAbort | 0x10 | Phone→ESP | Batalkan OTA |
| OtaEnterRecovery | 0x11 | Phone→ESP | Masuk recovery mode |

---

## Pairing & Security

### Numeric Comparison Pairing

```
1. ESP32 advertise "Chronchi-XXYY"
2. HP scan & connect
3. BLE stack trigger pairing
4. ESP32 tampilkan 6 digit kode di OLED
5. HP tampilkan dialog "Pair with Chronchi-XXYY?"
6. User tekan GPIO3 di ESP32 untuk konfirmasi
7. Pairing selesai, link terenkripsi
8. Bond disimpan di NVS (auto-reconnect next boot)
```

### Security Parameters

```cpp
ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_YES_NO;  // Display + confirm
ble_hs_cfg.sm_bonding = 1;      // Simpan bond
ble_hs_cfg.sm_mitm = 1;         // Man-in-the-middle protection
ble_hs_cfg.sm_sc = 1;           // Secure Connections (LE Secure)
ble_hs_cfg.sm_sc_only = 1;      // SC only (no legacy pairing)
ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
```

### Bond Storage

Bond disimpan di NVS namespace `ble_store`. Untuk reset pairing:
```
NVS erase → reboot → pairing ulang
```

---

## Connection Lifecycle

```
┌──────────────┐
│  IDLE        │ Tidak ada BLE
└──────┬───────┘
       │ Initialize()
       ▼
┌──────────────┐
│ ADVERTISING  │ Broadcast "Chronchi-XXYY"
└──────┬───────┘
       │ Phone connect
       ▼
┌──────────────┐
│ CONNECTED    │ BLE link established
└──────┬───────┘
       │ Security handshake
       ▼
┌──────────────┐
│ PAIRED       │ Link encrypted + bonded
└──────┬───────┘
       │ Phone subscribe TX notifications
       ▼
┌──────────────┐
│ SUBSCRIBED   │ Ready for data exchange
│ (ACTIVE)     │ ESP32 can send TX notifications
└──────┬───────┘
       │ Phone disconnect
       ▼
┌──────────────┐
│ ADVERTISING  │ Auto-restart advertising
└──────────────┘
```

---

## Wi-Fi + BLE Coexistence

### ESP32-C3 Single Radio

ESP32-C3 punya 1 hardware radio 2.4GHz yang di-share Wi-Fi dan BLE. Time-division multiplexing mengatur kapan masing-masing bisa TX/RX.

### Coexistence Mode

```c
CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y  // Software coexistence (default)
```

### Supported Scenarios

| Wi-Fi | BLE Scan | BLE Adv | BLE Connected |
|-------|----------|---------|---------------|
| STA Connected | ✅ Y | ✅ Y | ✅ Y |
| STA Scanning | ✅ Y | ✅ Y | ✅ Y |
| SoftAP | ⚠️ C1 | ⚠️ C1 | ⚠️ C1 |

**Y** = Stable, **C1** = Supported but unstable

### Performance Impact

- BLE connection interval 12ms → minimal impact ke Wi-Fi throughput
- BLE notification (small payload) → negligible Wi-Fi latency
- BLE OTA (large transfer) → significant Wi-Fi degradation

### RAM Usage

| Component | RAM |
|-----------|-----|
| NimBLE stack (1 conn, peripheral) | 40-50 KB |
| NimBLE buffers (ACL, HCI, MSYS) | 10-15 KB |
| Wi-Fi stack (minimum config) | 80-100 KB |
| **Total BLE + Wi-Fi** | **130-165 KB** |

---

## Xichi Mode: BLE + Wi-Fi Simultaneous

Di Xichi mode, BLE dan Wi-Fi berjalan bersamaan:

```
BLE:  Advertise → Phone connect → Terima notifikasi
Wi-Fi: Connect → WebSocket ke server → Kirim MCP → TTS

Kedua-duanya aktif bersamaan.
Coexistence otomatis di-handle oleh ESP-IDF.
```

**Tips untuk stabilitas:**
- BLE connection interval: 12ms (default)
- BLE hanya 1 connection (hemat resource)
- BLE tidak scan (peripheral only)
- Wi-Fi buffer minimum (hemat RAM)
- WebSocket dibuka saat perlu saja (bukan persistent)

---

## ChronchiMode BLE Implementation

```cpp
class ChronchiBle {
public:
    explicit ChronchiBle(ChronchiState& state);
    esp_err_t Initialize();   // Init NimBLE + GATT + advertise
    void Poll();              // Process pending responses
    bool ConfirmPairing();    // Accept numeric comparison

private:
    ChronchiState& state_;
    ChronchiProtocol protocol_;
    ChronchiOta ota_;
    
    std::atomic<uint16_t> connection_handle_{0xffff};
    std::atomic<bool> notify_subscribed_{false};
    std::atomic<bool> link_secure_{false};
    
    // Response queue (ACK, NACK, DeviceInfo)
    std::array<PendingResponse, 16> response_queue_;
    size_t response_head_ = 0;
    size_t response_tail_ = 0;
    size_t response_count_ = 0;
    std::mutex response_mutex_;
};
```

---

## Troubleshooting

| Masalah | Solusi |
|---------|--------|
| BLE tidak advertise | Cek NimBLE init, cek heap |
| Phone tidak bisa connect | Reset bond di HP & ESP32 |
| Pairing gagal | Cek security config, cek passkey |
| Data tidak masuk | Cek TX subscription, cek MTU |
| RAM rendah | Kurangi NimBLE buffer sizes |
| BLE disconnect saat Wi-Fi aktif | Normal, cek coexistence config |
| OTA gagal | Cek firmware size < max image |
