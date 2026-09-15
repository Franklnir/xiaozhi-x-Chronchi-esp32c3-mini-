# Mode: Xichi (Voice Notification Reader via Firebase)

**BootMode enum:** `BootMode::Xichi = 2`  
**Full name:** Xi(aozhi) + (Chron)chi  
**Persistent mode:** Tidak seperti Chronchi, Xichi tetap di Xichi sampai user ganti mode.

---

## Deskripsi

Xichi membacakan notifikasi HP via suara menggunakan **Firebase Realtime Database**. Ketika ada notifikasi baru di HP, ESP32 membacanya dari Firebase via Wi-Fi, lalu mengirimnya ke server Xiaozhi untuk di-TTS-kan melalui speaker MAX98357A.

**Tagline:** *"HP kirim notif ke cloud, ESP32 bacakan, kamu dengar aja."*

**Perbedaan dari Chronchi:**
- Chronchi: BLE only, no Wi-Fi, no audio
- Xichi: **Wi-Fi only, no BLE**, speaker output

---

## Aturan Utama

| Aturan | Detail |
|--------|--------|
| ❌ Tidak ada wake word | Wake word detection dimatikan total |
| ❌ Tidak masuk listening | Setelah TTS selesai → STANDBY, bukan listening |
| ❌ Tidak ada BLE | BLE tidak diinisialisasi di Xichi mode |
| ✅ Firebase polling | Poll setiap 5 detik via HTTPS |
| ✅ Semua notif dibacakan | Tidak ada filter, semua kategori dibacakan |
| ✅ Volume 100% | Selalu full volume |
| ✅ Push-to-talk aktif | GPIO3 bisa dipakai ngobrol manual |
| ✅ Notifikasi di-queue | Kalau speaking, notif baru masuk antrian |

---

## Provisioning Flow

### 1. BLE Connect di Chronchi Mode → Simpan API Secret

ESPBridge app mengirim `FIREBASE_CONFIG` packet via BLE saat pertama kali connect:

```json
{
  "type": "firebase_config",
  "url": "https://project-id-default-rtdb.firebaseio.com",
  "auth": "database-secret",
  "uid": "firebase-user-uid",
  "deviceId": "ESP-XXXX"
}
```

ESP32 menyimpan ke NVS (namespace: `firebase`):
- `url` (String, max 128 char)
- `auth` (String, max 256 char)
- `uid` (String, max 64 char)
- `device_id` (String, max 32 char)
- `configured` (uint8, value 1)

### 2. BLE Forget/Logout → Hapus API Secret

ESPBridge app mengirim `CLEAR_CONFIG` packet:
```json
{}
```

ESP32 menghapus semua data dari NVS namespace `firebase`.

### 3. Status di OLED Chronchi Mode

Tampilkan status API secret:
- "API SECRET: Saved" — config tersimpan, siap untuk Xichi mode
- "API SECRET: Cleared" — config dihapus
- "API SECRET: Not Set" — belum pernah di-setup

---

## Fitur Xichi Mode

### 1. Firebase Poller

Poll Firebase Realtime Database setiap 5 detik:

```
GET https://{DB_URL}/live/{UID}/{DEVICE_ID}/notification.json?auth={SECRET}
```

Response:
```json
{
  "sourceApp": "WhatsApp",
  "category": "MESSAGE",
  "primaryText": "Halo, rapat jam 2 siang ya",
  "secondaryText": "Budi",
  "timestamp": 1725800000
}
```

Polling logic:
1. Bandingkan `timestamp` dengan `last_timestamp`
2. Jika lebih baru → push ke notification queue
3. Jika sama atau lebih lama → skip

### 2. Notification → MCP Tool Call

Sama seperti sebelumnya, notifikasi dikirim ke server Xiaozhi:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/call",
  "params": {
    "name": "notification.read",
    "arguments": {
      "source_app": "WhatsApp",
      "category": "MESSAGE",
      "primary_text": "Halo, rapat jam 2 siang ya",
      "secondary_text": "Budi"
    }
  }
}
```

### 3. TTS Playback (Volume 100%)

Pipeline sama:
```
Server → WebSocket binary → Opus frames
  → AudioService::PushPacketToDecodeQueue()
  → Opus decoder → PCM
  → I2S output @ 24000 Hz
  → MAX98357A speaker (VOLUME 100%)
```

### 4. Post-TTS Behavior

Sama: paksa `kListeningModeManualStop` → setelah TTS selesai = STANDBY.

### 5. Notification Queue

Sama: kalau speaking, notif baru masuk antrian. Jeda 0.5 detik antar notif.

### 6. Push-to-Talk

Sama: GPIO3 untuk ngobrol manual. Setelah PTT selesai, tetap STANDBY.

### 7. No API Secret Handling

Jika NVS tidak ada config:
- OLED tampilkan "XICHI: No API Secret"
- Device tetap di STANDBY
- Push-to-talk tetap bisa dipakai
- User perlu masuk Chronchi mode dulu untuk provisioning

---

## RAM Budget

| State | Free Heap | Notes |
|-------|-----------|-------|
| Standby | 135-185 KB | Wi-Fi idle, no BLE |
| Open Channel | 100-150 KB | +WebSocket buffers |
| Speaking | 80-130 KB | +Opus decoder + audio buffers |

**Penghematan vs Xichi v2 (BLE + Wi-Fi):**
- BLE stack dimatikan: -50 KB
- BLE buffers dimatikan: -10 KB
- **Total hemat: ~60 KB**

---

## Latency

```
Firebase poll interval:     5 detik (rata-rata 2.5 detik)
HTTP GET to Firebase:       100-500 ms
JSON parse:                 ~1 ms
──────────────────────────────────
Detection latency:          0-5 detik

Open audio channel:         500-2000 ms
MCP tool call:              ~5 ms
Server LLM + TTS:           300-1500 ms
Audio streaming:            500-3000 ms
──────────────────────────────────
TOTAL: notif → suara        ~1-7 detik
```

---

## File Implementation

### New Files
```
main/xichi/
├── firebase_nvs.h      # NVS helper
├── firebase_nvs.cc     # Implementation
├── firebase_poller.h   # HTTP poller
├── firebase_poller.cc  # Implementation
├── xichi_mode.h        # XichiMode class
└── xichi_mode.cc       # Implementation
```

### Modified Files
```
main/chronchi/chronchi_protocol.h   # + FirebaseConfig, ClearConfig
main/chronchi/chronchi_protocol.cc  # + Parse firebase JSON
main/chronchi/chronchi_mode.cc      # + Callbacks, status display
main/CMakeLists.txt                 # + new source files
main/Kconfig.projbuild              # Updated description
```

### ESPBridge App Changes
```
BleConstants.kt         # + FIREBASE_CONFIG, CLEAR_CONFIG
TransportRouter.kt      # + sendFirebaseConfig(), sendClearConfig()
DeviceConnectionService.kt  # + Send config on connect
MainViewModel.kt        # + Clear config on forget/logout
```
