# Feature: Wi-Fi & Networking

---

## Overview

ESP32-C3 menggunakan Wi-Fi dalam mode Station (STA) untuk terhubung ke router dan mengakses server Xiaozhi. Di Xichi mode, Wi-Fi berjalan bersamaan dengan BLE (coexistence).

---

## Wi-Fi Configuration

```c
// Dari sdkconfig (optimized untuk hemat RAM)
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=3
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=6
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=32
CONFIG_ESP_WIFI_DYNAMIC_RX_MGMT_BUFFER=y
CONFIG_ESP_WIFI_DYNAMIC_RX_MGMT_BUF=1

// TCP/IP
CONFIG_LWIP_TCP_SND_BUF_DEFAULT=5760   // 5.6 KB per connection
CONFIG_LWIP_TCP_WND_DEFAULT=5760       // 5.6 KB window

// Coexistence
CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y    // Software coexistence
```

### RAM Usage by Configuration Rank

| Rank | Free Heap | TX Throughput | Use Case |
|------|-----------|---------------|----------|
| Minimum | 180 KB | 20 Mbps | Xichi mode (hemat RAM) |
| Default | 160 KB | 27 Mbps | Normal use |
| Iperf | 59 KB | 38 Mbps | Benchmark only |

**Xichi mode menggunakan Minimum rank** untuk hemat RAM.

---

## Wi-Fi Provisioning

### Setup via Hotspot

```
1. Boot device, tekan GPIO3 dalam 5 detik
2. Device buat hotspot "Xiaozhi-XXXX" (AP mode)
3. HP connect ke hotspot
4. Buka browser → http://192.168.4.1
5. Masukkan SSID & password WiFi rumah
6. Device simpan ke NVS & restart
7. Device connect ke WiFi rumah (STA mode)
```

### Saved Credentials

Disimpan di NVS namespace `wifi`:
- `ssid`: Nama WiFi
- `password`: Password WiFi

### Reset WiFi

**Via voice:** "reset wifi" / "ganti wifi" / "hapus wifi"  
**Via button:** Tahan GPIO2 selama 3 detik  
**Konfirmasi:** "ya" / "iya" dalam 15 detik

---

## Server Connection

### Protocol Selection

Device membaca settings untuk menentukan protocol:

```cpp
// Cek WebSocket settings
Settings ws_settings("websocket", false);
std::string ws_url = ws_settings.GetString("url");
if (!ws_url.empty()) {
    protocol = new WebsocketProtocol();
}
// Atau MQTT
Settings mqtt_settings("mqtt", false);
std::string mqtt_endpoint = mqtt_settings.GetString("endpoint");
if (!mqtt_endpoint.empty()) {
    protocol = new MqttProtocol();
}
```

### WebSocket Protocol

**Connection:**
```
URL: wss://api.xiaozhi.me/v1/chat
Headers:
  Authorization: Bearer <token>
  Protocol-Version: 3
  Device-Id: <MAC address>
  Client-Id: <UUID>
```

**Hello handshake:**
```json
// Device → Server
{
    "type": "hello",
    "version": 1,
    "features": {
        "mcp": true,
        "aec": false
    },
    "transport": "websocket",
    "audio_params": {
        "format": "opus",
        "sample_rate": 16000,
        "channels": 1,
        "frame_duration": 60
    }
}

// Server → Device
{
    "type": "hello",
    "transport": "websocket",
    "session_id": "xxx",
    "audio_params": {
        "format": "opus",
        "sample_rate": 24000,
        "channels": 1,
        "frame_duration": 60
    }
}
```

### MQTT Protocol

**Broker:** Dikonfigurasi via settings  
**Topics:**
- Publish: `devices/<device-id>/audio`
- Subscribe: `devices/<device-id>/audio/response`
- Publish: `devices/<device-id>/text`
- Subscribe: `devices/<device-id>/text/response`

**Catatan:** MQTT lebih cocok untuk IoT, WebSocket lebih cocok untuk real-time audio.

---

## Connection Lifecycle

```
┌──────────┐
│DISCONNECTED│
└─────┬──────┘
      │ StartNetwork()
      ▼
┌──────────┐
│ SCANNING │ Cari WiFi tersimpan
└─────┬──────┘
      │ SSID ditemukan
      ▼
┌──────────┐
│CONNECTING│ Auth ke WiFi AP
└─────┬──────┘
      │ Got IP
      ▼
┌──────────┐
│CONNECTED │ DHCP success, siap akses server
└──────────┘
```

---

## Wi-Fi + BLE Coexistence

### ESP32-C3 Single Radio

ESP32-C3 punya 1 hardware radio. Wi-Fi dan BLE share radio via time-division multiplexing.

### Coexistence Scenarios

| Scenario | Support | Performance |
|----------|---------|-------------|
| Wi-Fi STA + BLE Advertise | ✅ Stable | Minimal impact |
| Wi-Fi STA + BLE Connected | ✅ Stable | Some latency jitter |
| Wi-Fi STA + BLE Data Transfer | ✅ OK | Reduced throughput |
| Wi-Fi SoftAP + BLE Connected | ⚠️ C1 | Unstable |

### Configuration

```c
CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y  // Software coexistence (default)
CONFIG_ESP_COEX_POWER_MANAGEMENT=n   // Disable power management for stability
```

### Performance Tips

1. **BLE connection interval:** 12ms (default, good balance)
2. **BLE only 1 connection:** Hemat resource
3. **BLE peripheral only:** Tidak scan/observe
4. **Wi-Fi buffer minimum:** Hemat RAM
5. **WebSocket persistent:** Buka sekali, tutup saat idle

---

## Error Handling

### Wi-Fi Disconnect

```cpp
// Auto-reconnect
void OnWifiDisconnected() {
    // Tunggu 5 detik
    vTaskDelay(pdMS_TO_TICKS(5000));
    // Scan & reconnect
    esp_wifi_connect();
}
```

### Server Disconnect

```cpp
// WebSocket close handler
void OnWebSocketClosed() {
    // Reset protocol state
    protocol_->Reset();
    // Notify application
    on_audio_channel_closed_();
}
```

### Timeout

```cpp
// Connection timeout
#define CONNECT_TIMEOUT_MS 10000    // 10 detik
#define RESPONSE_TIMEOUT_MS 120000  // 2 menit (server response)
```

---

## Power Save

### Levels

| Level | Description | Use Case |
|-------|-------------|----------|
| PERFORMANCE | Full speed, no power save | Active audio |
| LOW_POWER | Wi-Fi power save active | Idle/standby |

```cpp
// Saat idle
Board::GetInstance().SetPowerSaveLevel(PowerSaveLevel::LOW_POWER);

// Saat mulai recording/speaking
Board::GetInstance().SetPowerSaveLevel(PowerSaveLevel::PERFORMANCE);
```

---

## Troubleshooting

| Masalah | Solusi |
|---------|--------|
| WiFi tidak connect | Reset WiFi credentials, setup ulang |
| WiFi disconnect terus | Cek sinyal, kurangi WiFi buffer |
| Server tidak reachable | Cek internet, cek server status, cek token |
| WebSocket timeout | Cek koneksi, cek server, reconnect |
| RAM rendah | Kurangi WiFi buffer, matikan fitur |
| Latency tinggi | Cek coexistence, kurangi BLE activity |
| MQTT disconnect | Cek broker, cek keepalive, reconnect |
