# Mode: Chronchi (Phone Companion via BLE)

**BootMode enum:** `BootMode::Chronchi = 1`  
**One-shot mode:** Chronchi adalah mode sekali-boot. Setelah restart, kembali ke Xiaozhi.

---

## Deskripsi

Chronchi adalah mode BLE-only yang menghubungkan ESP32 ke HP Android via Bluetooth Low Energy. ESP32 berfungsi sebagai "smart watch" mini — menampilkan notifikasi, status HP, cuaca, navigasi, dan info lainnya di OLED. TIDAK ada Wi-Fi, TIDAK ada audio, TIDAK ada voice AI.

---

## Fitur

### 1. BLE Connection

**Protocol:** ESPBridge V1 (framed JSON over GATT)  
**Security:** BLE Secure Connections (SC) dengan numeric comparison pairing  
**MTU:** 247 byte (default)  
**Max JSON:** 768 byte per message (bisa di-fragment)

**GATT Service UUID:** `7c9e0001-6f2f-4d4d-9f25-0d7fd4f0a001`

| Characteristic | UUID | Direction | Fungsi |
|----------------|------|-----------|--------|
| RX | `7c9e0002-...` | Phone → ESP32 | Terima data dari HP |
| TX | `7c9e0003-...` | ESP32 → Phone | Kirim response ke HP |

**Device name:** `Chronchi-XXYY` (XXYY = 2 byte terakhir MAC)

### 2. Pairing

**Mode:** Numeric comparison (6 digit code)  
**Flow:**
```
1. ESP32 advertise sebagai "Chronchi-XXYY"
2. HP scan & connect
3. ESP32 tampilin kode 6 digit di OLED: "Kode 123456"
4. User tekan tombol GPIO3 untuk konfirmasi
5. Pairing selesai, koneksi terenkripsi
```

**Bonding:** Disimpan di NVS, auto-reconnect saat boot berikutnya

### 3. Packet Types (Phone → ESP32)

#### 3.1 TimeSync (0x01)
Sinkronisasi jam dari HP.

```json
{
    "epochSeconds": 1694000000,
    "utcOffsetMinutes": 420
}
```
**Effect:** Update jam di OLED (WIB = UTC+7 = 420 menit)

#### 3.2 PhoneStatus (0x02)
Status baterai HP.

```json
{
    "batteryLevel": 85,
    "charging": false
}
```
**Effect:** Tampilkan ikon baterai HP di status bar

#### 3.3 NetworkStatus (0x03)
Status jaringan HP.

```json
{
    "transport": "cellular",
    "generation": "4G",
    "signalLevel": 3
}
```
**Values:**
- `transport`: "wifi", "cellular", "offline"
- `generation`: "2G", "3G", "4G", "5G", "LTE"
- `signalLevel`: 0-4 (0 = no signal, 4 = full)

#### 3.4 Weather (0x04)
Data cuaca dari HP.

```json
{
    "temperatureC": 32,
    "location": "Jakarta"
}
```

#### 3.5 Notification (0x05)
Notifikasi masuk dari HP.

```json
{
    "category": "MESSAGE",
    "sourceApp": "WhatsApp",
    "primaryText": "Budi: otw ya",
    "secondaryText": "5 menit lagi sampai",
    "tertiaryText": "",
    "time": "14:30",
    "timestamp": 1694000000000
}
```

**Kategori notifikasi:**

| Kategori | Ikon | Warna | Contoh |
|----------|------|-------|--------|
| `MESSAGE` | 💬 Chat | Biru | WhatsApp, Telegram, SMS |
| `EMAIL` | 📧 Email | Hijau | Gmail, Outlook |
| `PROFESSIONAL` | 💼 Work | Biru tua | Slack, Teams |
| `PAYMENT` | 💳 Payment | Hijau | GoPay, OVO, DANA |
| `ORDER` | 📦 Order | Orange | Shopee, Tokopedia, Grab |
| `SYSTEM` | ⚙️ System | Abu-abu | System notifications |

**Order status (sub-status untuk ORDER):**

| Status | Label Indonesia |
|--------|----------------|
| `CONFIRMED` / `PESANAN DIKONFIRMASI` | Dikonfirmasi |
| `PACKED` / `SEDANG DIKEMAS` | Dikemas |
| `SHIPPED` / `PESANAN DIKIRIM` | Dikirim |
| `IN_TRANSIT` / `DALAM PERJALANAN` | Dalam Perjalanan |
| `OUT_FOR_DELIVERY` / `SEDANG DIANTAR` | Sedang Diantar |
| `DELIVERED` / `PESANAN DITERIMA` | Diterima |
| `CANCELLED` / `PESANAN DIBATALKAN` | Dibatalkan |

**Payment direction:**
- `INCOMING` → Uang masuk (hijau)
- `OUTGOING` → Uang keluar (merah)

#### 3.6 Navigation (0x06)
Navigasi dari Google Maps / Waze.

```json
{
    "active": true,
    "maneuver": "TURN_LEFT",
    "distanceText": "200 m",
    "roadName": "Jl. Sudirman",
    "destinationDistanceText": "2.5 km",
    "time": "14:35"
}
```

**Maneuver types:**

| Maneuver | Label | Ikon |
|----------|-------|------|
| `TURN_LEFT` / `KIRI` | Belok kiri | ← |
| `TURN_RIGHT` / `KANAN` | Belok kanan | → |
| `SLIGHT_LEFT` / `SERONG KIRI` | Serong kiri | ↰ |
| `SLIGHT_RIGHT` / `SERONG KANAN` | Serong kanan | ↱ |
| `STRAIGHT` / `LURUS` | Lurus | ↑ |
| `ROUNDABOUT` / `BUNDARAN` | Bundaran | ↻ |
| `ARRIVE` / `SAMPAI` | Sampai | 🏁 |

Untuk stop navigasi: `{"active": false}`

#### 3.7 Location (0x07)
Koordinat GPS dari HP.

```json
{
    "latitude": -6.2088,
    "longitude": 106.8456
}
```
**Note:** Diterima tapi tidak digunakan di OLED (location info dari Weather lebih human-readable).

#### 3.8 Command (0x08)
Command dari HP ke ESP32.

```json
{
    "action": "DISMISS"
}
```

**Actions:**
- `DISMISS` → Hapus notifikasi/transient screen
- `NAVIGATION_STOP` → Stop navigasi

#### 3.9 DeviceStatus (0x09)
Capability request dari HP.

```json
{}
```
**Response:** ESP32 kirim balik info device (ID, firmware version, board name, OTA capability, security status, partition slot).

### 4. Packet Types (ESP32 → Phone)

#### 4.1 Ack
```json
{"ok": true}
```

#### 4.2 Error
```json
{"ok": false, "error": "unknown packet type"}
```

#### 4.3 DeviceStatus Response
```json
{
    "id": "CH-A1B2",
    "name": "Chronchi-A1B2",
    "fw": "1.0.0",
    "board": "esp32c3-inmp441",
    "ota": true,
    "max": 2621440,
    "secure": true,
    "slot": "main",
    "update": "ota",
    "role": "main"
}
```

### 5. OTA Update via BLE

ESP32 bisa menerima firmware update via BLE dari HP.

**Flow:**
```
1. HP kirim OtaBegin (size, checksum)
2. HP kirim OtaChunk (data packets)
3. HP kirim OtaEnd
4. ESP32 verify & write ke OTA partition
5. ESP32 kirim ack dengan reboot flag
6. ESP32 restart ke firmware baru
```

**Max image size:** ~2.5 MB (tergantung partition)

### 6. Display (OLED)

**Home screen:**
```
┌─────────────────────┐
│ ⚡85%  📶4G  🕐14:30│  Status bar
│                     │
│    😊               │  Face/emoji
│                     │
│ 28°C  Jakarta       │  Cuaca + lokasi
│                     │
│ 📱HP:78% 🔋CHG      │  Baterai HP
└─────────────────────┘
```

**Notification screen:**
```
┌─────────────────────┐
│ WhatsApp         14:30│  Source app + time
│                     │
│ Budi: otw ya        │  Primary text
│ 5 menit lagi sampai │  Secondary text
│                     │
└─────────────────────┘
```

**Navigation screen:**
```
┌─────────────────────┐
│ NAVIGATION       14:35│
│                     │
│ ← Belok kiri        │  Maneuver + direction
│ 200 m               │  Distance
│ Jl. Sudirman        │  Road name
│ 2.5 km ke tujuan    │  Remaining distance
└─────────────────────┘
```

**Screen priority (tinggi → rendah):**
1. Navigation (timeout: sampai `active=false`)
2. Payment (timeout: 30 detik)
3. Order (timeout: 30 detik)
4. Message (timeout: 20 detik)
5. Professional (timeout: 20 detik)
6. System (timeout: 10 detik)
7. Home (selalu ada di background)

### 7. Battery Monitor

**Hardware:** ADC GPIO1 dengan voltage divider 100k/100k  
**Cell:** Single-cell Li-ion  
**Charging detection:** Via ADC level change  
**Refresh:** Setiap 5 detik

---

## State Machine

```
┌───────────┐
│  STARTUP  │ Init BLE + OLED + Battery
└─────┬─────┘
      ▼
┌───────────┐  Phone connect     ┌────────────┐
│ADVERTISING│ ─────────────────► │ CONNECTED  │
│  (BLE)    │ ◄───────────────── │ (paired)   │
└───────────┘  Phone disconnect  └─────┬──────┘
                                       │ Subscribe TX
                                       ▼
                               ┌────────────┐
                               │ SUBSCRIBED │
                               │ (active)   │
                               └────────────┘
```

---

## Limitasi

| Limitasi | Alasan |
|----------|--------|
| Tidak ada Wi-Fi | Radio BLE-only, hemat RAM |
| Tidak ada audio | Tidak ada speaker/mic usage |
| Tidak ada voice AI | Tidak ada server connection |
| Hanya 1 BLE connection | Config: MAX_CONNECTIONS=1 |
| JSON max 768 byte | MTU limit |
| One-shot boot | Restart → kembali ke Xiaozhi |

---

## ESPBridge Android App

App companion yang berjalan di HP Android.

**Fitur:**
- Kirim notifikasi ke ESP32
- Kirim status HP (baterai, sinyal, cuaca)
- Kirim navigasi dari Google Maps
- OTA firmware update via BLE
- Auto-reconnect ke device yang sudah di-pair

**Source code:** Tersedia, bisa dimodifikasi

---

## Troubleshooting

| Masalah | Solusi |
|---------|--------|
| BLE tidak connect | Reset pairing, hapus bond di HP & ESP32 |
| Notifikasi tidak masuk | Cek permission notifikasi di Android |
| Jam tidak sinkron | Tunggu TimeSync dari HP, atau set manual |
| OTA gagal | Pastikan ukuran firmware < max image size |
| OLED tidak tampil | Cek wiring SDA→GPIO8, SCL→GPIO9 |
