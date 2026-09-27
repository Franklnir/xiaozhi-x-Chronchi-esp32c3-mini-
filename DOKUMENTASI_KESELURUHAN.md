# Dokumentasi Lengkap: xiaozhi-AI-esp32c3-mini-main

> Firmware ESP32-C3 untuk AI Smart Speaker berbasis Xiaozhi AI dengan MCP Protocol, WebSocket, Audio Streaming, dan Dual-Mode Operation.

---

## Daftar Isi

1. [Ringkasan Proyek](#1-ringkasan-proyek)
2. [Arsitektur Sistem](#2-arsitektur-sistem)
3. [Teknologi yang Digunakan](#3-teknologi-yang-digunakan)
4. [Fitur Utama](#4-fitur-utama)
5. [Komponen Hardware](#5-komponen-hardware)
6. [Komponen Software](#6-komponen-software)
7. [Protokol Komunikasi](#7-protokol-komunikasi)
8. [Audio Pipeline](#8-audio-pipeline)
9. [YouTube Audio Streaming](#9-youtube-audio-streaming)
10. [MCP Server (On-Device)](#10-mcp-server-on-device)
11. [Sistem Operasi dan Task Management](#11-sistem-operasi-dan-task-management)
12. [Adaptive Bitrate Streaming](#12-adaptive-bitrate-streaming)
13. [Struktur File dan Direktori](#13-struktur-file-dan-direktori)
14. [Konfigurasi Pin dan Hardware](#14-konfigurasi-pin-dan-hardware)
15. [Alur Kerja Lengkap](#15-alur-kerja-lengkap)
16. [Daftar MCP Tools](#16-daftar-mcp-tools)
17. [Error Handling dan Recovery](#17-error-handling-dan-recovery)
18. [Power Management](#18-power-management)
19. [Troubleshooting](#19-troubleshooting)

---

## 1. Ringkasan Proyek

**Nama:** Xiaozhi AI - ESP32-C3 Edition
**Platform:** ESP32-C3 (4MB Flash, ~400KB RAM)
**Framework:** ESP-IDF v5.5+
**Bahasa:** C++ (embedded)
**Biaya Hardware:** ~Rp 91.000 (~$5.80 USD)

Proyek ini adalah firmware untuk ESP32-C3 yang mengubahnya menjadi AI Smart Speaker dengan kemampuan:
- Voice conversation dengan AI (via WebSocket ke Xiaozhi Cloud)
- MCP tool calling (JSON-RPC 2.0)
- YouTube audio streaming dengan adaptive bitrate
- Dual-mode: Xiaozhi AI + Chronos/Mochi BLE
- OLED display, wake word detection, hands-free mode

**Alasan Penggunaan ESP32-C3:**
ESP32-C3 dipilih karena harganya sangat murah (~Rp 35.000), hemat daya, sudah punya WiFi, dan memiliki I2S peripheral yang cukup untuk mikrofon + speaker. Keterbatasan RAM (~400KB) memaksa desain streaming yang chunked dan adaptive — yang justru menjadi topik penelitian yang menarik.

---

## 2. Arsitektur Sistem

```
┌─────────────────────────────────────────────────────────────────┐
│                        USER (Manusia)                           │
│                     "Putar Despacito"                           │
└──────────────────────────┬──────────────────────────────────────┘
                           │ Voice Input
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│                     ESP32-C3 Device                             │
│  ┌──────────┐  ┌──────────┐  ┌───────────┐  ┌──────────────┐  │
│  │ INMP441  │→ │ Opus     │→ │ WebSocket │→ │ Xiaozhi Cloud│  │
│  │ (Mic)    │  │ Encoder  │  │ Protocol  │  │ (AI/LLM)     │  │
│  └──────────┘  └──────────┘  └─────┬─────┘  └──────┬───────┘  │
│                                     │               │          │
│  ┌──────────┐  ┌──────────┐  ┌─────┴─────┐  ┌──────┴───────┐  │
│  │ MAX98357A│← │ Opus     │← │ MCP       │← │ MCP Server   │  │
│  │ (Speaker)│  │ Decoder  │  │ Parser    │  │ (Backend)    │  │
│  └──────────┘  └──────────┘  └───────────┘  └──────────────┘  │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │              YouTube Audio Poller                        │  │
│  │  HTTP Poll → Command Queue → HTTP Stream → Opus Decode   │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌──────────┐  ┌──────────┐  ┌───────────┐                    │
│  │ SSD1306  │  │ Wake     │  │ Mode      │                    │
│  │ OLED     │  │ Word     │  │ Selector  │                    │
│  │ Display  │  │ Detector │  │ (Dual)    │                    │
│  └──────────┘  └──────────┘  └───────────┘                    │
└─────────────────────────────────────────────────────────────────┘
```

### Dua Jalur Komunikasi

**Jalur 1: AI Conversation (WebSocket)**
```
ESP32 ←→ WebSocket ←→ wss://api.xiaozhi.me ←→ LLM AI
```
- Real-time voice conversation
- MCP tool calls embedded dalam WebSocket messages
- Binary audio frames (Opus) + JSON text messages

**Jalur 2: YouTube Streaming (HTTP Polling + HTTP Stream)**
```
ESP32 → HTTP GET /commands?mac=XX → Backend Custom
ESP32 ← HTTP Response (command queue) ← Backend Custom
ESP32 → HTTP GET /stream/<vid>?br=12k → Backend Custom
ESP32 ← HTTP Chunked (Opus frames) ← Backend Custom
```
- Command delivery via polling
- Audio delivery via HTTP chunked streaming

---

## 3. Teknologi yang Digunakan

| Teknologi | Versi | Fungsi | Alasan Pemilihan |
|-----------|-------|--------|-------------------|
| **ESP-IDF** | v5.5.2 | Framework RTOS untuk ESP32 | Standar Espressif, full control hardware |
| **C++** | C++17 | Bahasa pemrograman | Performance, memory control, OOP |
| **FreeRTOS** | (bundled) | Real-time OS | Task scheduling, queue, mutex, event groups |
| **Opus Codec** | libopus | Audio encoder/decoder | Codec paling efisien untuk voice, low latency, royalty-free |
| **WebSocket** | RFC 6455 | Full-duplex communication | Real-time bidirectional untuk AI chat |
| **HTTP/1.1** | chunked | YouTube audio streaming | Simple, reliable, cocok untuk embedded polling |
| **JSON-RPC** | 2.0 | MCP message format | Standar industri untuk tool invocation |
| **cJSON** | (bundled) | JSON parser | Lightweight, embedded-friendly |
| **I2S** | ESP32 driver | Audio interface | Hardware peripheral untuk mic + speaker |
| **NVS** | ESP-IDF | Non-volatile storage | Simpan WiFi config, settings, server domain |
| **SSD1306** | 128x64 | OLED display | I2C, hemat pin, murah |
| **ESP Wake Word** | ESP-SR | Wake word detection | On-device, tanpa cloud |

### Mengapa Teknologi Ini Dipilih?

1. **ESP-IDF vs Arduino**: ESP-IDF memberikan full control atas memory, task priority, dan hardware peripheral. Arduino abstraction layer terlalu tinggi untuk embedded streaming yang butuh presisi timing.

2. **Opus vs MP3/AAC**: Opus memiliki latency paling rendah (~26ms frame minimum, 60ms di project ini), compression ratio sangat baik untuk voice, dan zero royalty. Untuk embedded device dengan RAM terbatas, Opus adalah pilihan terbaik.

3. **WebSocket vs MQTT untuk AI Chat**: WebSocket memberikan full-duplex real-time communication yang dibutuhkan untuk voice conversation. MQTT lebih cocok untuk IoT telemetry, bukan streaming audio.

4. **HTTP Polling vs WebSocket untuk YouTube Commands**: ESP32-C3 sudah memakai 1 WebSocket untuk AI chat. Menambah WebSocket kedua terlalu berat untuk RAM 400KB. HTTP polling lebih hemat resource untuk command delivery yang tidak real-time critical.

5. **FreeRTOS vs Bare Metal**: Task scheduling, queue management, dan mutex sangat dibutuhkan untuk mengelola audio pipeline (encode, decode, playback) secara concurrent.

---

## 4. Fitur Utama

### 4.1 Voice AI Conversation
- Push-to-talk (tombol) atau hands-free (auto-listen)
- Wake word detection ("Hi Lexin" / "Hi Jason")
- Opus encoded audio bidirectional
- STT (Speech-to-Text) di server, LLM response, TTS (Text-to-Speech) playback

### 4.2 MCP Protocol (On-Device)
- JSON-RPC 2.0 server berjalan di ESP32
- `initialize`, `tools/list`, `tools/call` methods
- Device capability discovery oleh server
- Tool execution callbacks ke hardware

### 4.3 YouTube Audio Streaming
- Voice command: "Putar Despacito" → MCP tool call → stream audio
- HTTP polling untuk command delivery
- HTTP chunked streaming untuk audio delivery
- Adaptive bitrate (6k/8k/12k/16k) berdasarkan RSSI + free RAM
- Buffer starvation detection dan automatic downshift
- Ogg demuxing on-device, raw Opus frame playback

### 4.4 Dual-Mode Operation
- **Xiaozhi Mode**: Full AI conversation, WiFi, audio streaming
- **Chronos/Mochi Mode**: BLE-based, low power
- Mode switching via long-press button (GPIO3)
- Mode disimpan di NVS (persistent across reboot)

### 4.5 Display System
- SSD1306 OLED 128x64 via I2C
- Status display (connecting, listening, speaking)
- Song title display saat YouTube streaming
- Volume level, battery percentage

### 4.6 Power Management
- Auto power-off audio peripheral saat idle (30 detik)
- Battery monitoring via ADC (single-cell Li-ion)
- WiFi power save modes

---

## 5. Komponen Hardware

### 5.1 Bill of Materials

| Komponen | Fungsi | Harga (IDR) | Pin ESP32-C3 |
|----------|--------|-------------|--------------|
| ESP32-C3 DevKit | Mikrokontroler utama | Rp 35.000 | - |
| INMP441 | I2S MEMS Microphone | Rp 25.000 | GPIO4(SD), GPIO5(SCK), GPIO6(WS) |
| MAX98357A | I2S Class D Amplifier (3W) | Rp 20.000 | GPIO7(DIN), GPIO5(BCLK), GPIO6(LRC) |
| Speaker 3W 4Ω | Audio output | Rp 10.000 | ke MAX98357A |
| Push Button | Mode/chat trigger | Rp 1.000 | GPIO3 |
| SSD1306 OLED | Display 128x64 (opsional) | Rp 25.000 | GPIO0(SDA), GPIO10(SCL) |

### 5.2 Wiring Diagram

```
ESP32-C3          INMP441 (Mic)      MAX98357A (Speaker)
─────────         ─────────────      ───────────────────
GPIO 5  ────────► SCK                BCLK
GPIO 6  ────────► WS                 LRC
GPIO 4  ────────► SD
GPIO 7  ──────────────────────────►  DIN
3.3V    ────────► VDD
5V      ──────────────────────────►  VIN
GND     ────────► GND                GND
        ────────► L/R → GND

ESP32-C3          SSD1306 OLED (Opsional)
─────────         ──────────────────────
GPIO 0  ────────► SDA
GPIO 10 ────────► SCL
3.3V    ────────► VCC
GND     ────────► GND
```

**Catatan Penting:**
- ESP32-C3 hanya punya **1 I2S peripheral**, jadi mic dan speaker SHARE clock (BCLK=GPIO5, WS/LRC=GPIO6). Data terpisah: DIN(mic)=GPIO4, DOUT(speaker)=GPIO7.
- MAX98357A HARUS pakai **5V**, bukan 3.3V (akan clipping/distorsi).
- INMP441 L/R pin harus ke **GND** (left channel).

### 5.3 Kenapa Komponen Ini?

1. **INMP441**: MEMS microphone dengan SNR tinggi (61dB), digital I2S output, tidak butuh ADC analog. Cocok untuk voice recognition.

2. **MAX98357A**: Class D amplifier, efisiensi tinggi (>90%), langsung drive speaker 3W tanpa komponen tambahan. I2S input menghilangkan kebutuhan DAC external.

3. **SSD1306**: OLED display paling umum di embedded, I2C (hemat pin), konsumsi daya rendah, library tersedia luas.

4. **ESP32-C3 vs ESP32**: Lebih murah, RISC-V core, cukup untuk audio streaming. Trade-off: hanya 1 I2S peripheral (harus share clock), lebih sedikit RAM.

---

## 6. Komponen Software

### 6.1 Struktur Kode Utama

```
main/
├── main.cc                    # Entry point, init semua komponen
├── application.cc/.h          # State machine utama, message routing
├── mcp_server.cc/.h           # MCP JSON-RPC 2.0 server (on-device)
├── youtube_audio_poller.cc/.h # YouTube polling + HTTP stream consumer
├── settings_manager.cc/.h     # NVS settings (WiFi, server, mode)
├── web_settings_server.cc     # WiFi captive portal (AP mode)
├── sleep_manager.cc/.h        # Deep sleep management
├── cellular_module.cc/.h      # Cellular modem support (opsional)
│
├── audio/
│   ├── audio_service.cc/.h    # Audio pipeline utama (encode/decode/playback)
│   ├── audio_codec.h          # Abstract codec interface
│   ├── codecs/
│   │   └── no_audio_codec.cc/.h  # Null codec (no hardware)
│   └── wake_words/
│       └── esp_wake_word.cc/.h   # Wake word detection
│
├── boards/
│   └── esp32c3-inmp441/
│       ├── config.h           # Pin definitions, audio params
│       └── esp32c3_inmp441_board.cc  # Board-specific init
│
├── protocols/
│   ├── protocol.h             # Abstract protocol interface
│   └── websocket_protocol.cc/.h  # WebSocket implementation
│
├── display/
│   ├── display.h              # Abstract display interface
│   ├── oled_display.cc/.h     # SSD1306 OLED driver
│   └── lvgl_display/          # LVGL graphics (opsional)
│
├── mode/
│   ├── mode_selector.cc/.h    # Mode switching logic
│   ├── mode_store.cc/.h       # NVS mode persistence
│   └── boot_mode.h            # Boot mode definitions
│
└── chronchi/                  # Chronos/Mochi BLE mode
    ├── chronchi_protocol.cc/.h
    ├── chronchi_mode.cc/.h
    ├── chronchi_ble.cc/.h
    └── chronchi_state.cc/.h
```

### 6.2 Peran Setiap Komponen

| Komponen | File | Fungsi | Alasan |
|----------|------|--------|--------|
| **Application** | `application.cc` | State machine utama, message routing, mode switching | Orchestrator pusat yang mengatur semua task dan state |
| **McpServer** | `mcp_server.cc` | Parse JSON-RPC 2.0, register tools, execute callbacks | Standar MCP untuk IoT control oleh LLM |
| **YouTubeAudioPoller** | `youtube_audio_poller.cc` | HTTP poll commands, HTTP stream audio, adaptive bitrate | Pola polling karena ESP32 tidak bisa maintain 2 WebSocket |
| **AudioService** | `audio_service.cc` | Mic capture, Opus encode/decode, speaker playback | Pipeline audio terpusat, manage queue dan task |
| **WebsocketProtocol** | `websocket_protocol.cc` | WebSocket connection ke Xiaozhi Cloud | Full-duplex untuk voice + text + MCP |
| **ModeSelector** | `mode_selector.cc` | Switch antara Xiaozhi dan Chronos mode | Dual-purpose device dari 1 tombol |
| **SettingsManager** | `settings_manager.cc` | NVS read/write untuk config | Persist settings across reboot |
| **OledDisplay** | `oled_display.cc` | Render text ke SSD1306 | User feedback (status, lagu, volume) |
| **WakeWord** | `esp_wake_word.cc` | On-device wake word detection | Hands-free activation tanpa cloud |

---

## 7. Protokol Komunikasi

### 7.1 WebSocket Protocol (AI Chat + MCP)

**Koneksi ke:** `wss://api.xiaozhi.me`

**Request Headers:**
```
Authorization: Bearer <token>
Protocol-Version: 3
Device-Id: <MAC address>
Client-Id: <UUID generated>
```

**Handshake:**
```
ESP32 → Server: {"type":"hello","version":1,"features":{"mcp":true},"transport":"websocket","audio_params":{"format":"opus","sample_rate":16000,"channels":1,"frame_duration":60}}
Server → ESP32: {"type":"hello","transport":"websocket","session_id":"xxx","audio_params":{"format":"opus","sample_rate":24000}}
```

**Binary Protocol (v3):**
```
┌────────┬──────────┬───────────────┬─────────────────┐
│ type   │ reserved │ payload_size  │ payload (Opus)  │
│ 1 byte │ 1 byte   │ 2 bytes (BE)  │ N bytes         │
└────────┴──────────┴───────────────┴─────────────────┘
```

**JSON Message Types:**

| Direction | Type | Fungsi |
|-----------|------|--------|
| ESP32→Server | `hello` | Handshake, capability announcement |
| ESP32→Server | `listen` | Start/stop recording |
| ESP32→Server | `abort` | Stop current TTS playback |
| ESP32→Server | `mcp` | MCP tool result (JSON-RPC 2.0 response) |
| Server→ESP32 | `hello` | Handshake response, session_id |
| Server→ESP32 | `stt` | Speech-to-text result |
| Server→ESP32 | `tts` | TTS start/stop/sentence events |
| Server→ESP32 | `llm` | Emotion/UI state |
| Server→ESP32 | `mcp` | MCP tool call (JSON-RPC 2.0 request) |
| Server→ESP32 | `system` | Reboot command |

### 7.2 HTTP Protocol (YouTube Streaming)

**Polling Endpoint:**
```
GET /api/device/audio/commands?mac=<MAC_ADDRESS>
Authorization: ESP32-XiaoZhi (User-Agent)

Response: {"commands": [{"id": 123, "video_id": "kJQP7kiw5Fk", "title": "Despacito"}]}
```

**Acknowledgment:**
```
POST /api/device/audio/ack
Content-Type: application/json
{"command_id": 123, "mac": "<MAC_ADDRESS>"}
```

**Status Report:**
```
POST /api/device/audio/status
Content-Type: application/json
{"status": "playing", "mac": "<MAC_ADDRESS>", "video_id": "kJQP7kiw5Fk"}
```

**Audio Stream:**
```
GET /api/audio/stream/<video_id>?br=12k&start=0.00
Device-Id: <MAC_ADDRESS>

Response: HTTP/1.1 200 OK
          Transfer-Encoding: chunked
          Content-Type: audio/ogg

          [binary Opus frames in Ogg container, continuous]
```

---

## 8. Audio Pipeline

### 8.1 Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                      AudioService                               │
│                                                                 │
│  MIC PATH (ESP32 → Server):                                     │
│  ┌────────┐   ┌──────────┐   ┌────────┐   ┌─────────┐         │
│  │ INMP441│──►│Resampler │──►│ Opus   │──►│SendQueue│──►Server│
│  │ (I2S)  │   │24k→16k   │   │Encoder │   │(3 pkt)  │         │
│  └────────┘   └──────────┘   └────────┘   └─────────┘         │
│                                                                 │
│  SPEAKER PATH (Server → ESP32):                                 │
│  ┌─────────┐   ┌──────────┐   ┌────────┐   ┌─────────┐        │
│  │MAX98357A│◄──│Resampler │◄──│ Opus   │◄──│DecodeQ  │◄─Server│
│  │ (I2S)   │   │16k→24k   │   │Decoder │   │(10 pkt) │        │
│  └─────────┘   └──────────┘   └────────┘   └─────────┘        │
│                                                                 │
│  YOUTUBE PATH (Backend → ESP32):                                │
│  ┌─────────┐   ┌──────────┐   ┌────────┐   ┌─────────┐        │
│  │MAX98357A│◄──│Resampler │◄──│ Opus   │◄──│HTTP     │◄─HTTP  │
│  │ (I2S)   │   │24k→24k   │   │Decoder │   │Chunked  │  Stream│
│  └─────────┘   └──────────┘   └────────┘   └─────────┘        │
└─────────────────────────────────────────────────────────────────┘
```

### 8.2 Audio Parameters

| Parameter | Nilai | Alasan |
|-----------|-------|--------|
| Sample Rate (mic) | 24000 Hz | Balance quality vs bandwidth |
| Sample Rate (speaker) | 24000 Hz | Cocok untuk music + voice |
| Sample Rate (server) | 16000 Hz | Xiaozhi cloud default |
| Channels | 1 (mono) | Hemat bandwidth, cukup untuk speaker kecil |
| Frame Duration | 60ms | Balance latency vs overhead |
| Codec | Opus | Best compression untuk voice, low latency |
| Bit Depth | 16-bit | Standard audio quality |

### 8.3 FreeRTOS Tasks

| Task | Stack | Priority | Core | Fungsi |
|------|-------|----------|------|--------|
| `audio_input` | 6KB | 8 | 0 | Baca mic, proses audio |
| `audio_output` | 4KB | 4 | any | Tulis ke speaker |
| `opus_codec` | 26KB | 2 | any | Encode/decode Opus |
| `yt_poller` | 5.6KB | 2 | any | YouTube polling + streaming |

### 8.4 Queue Configuration (ESP32-C3 Specific)

| Queue | Max Size | Content | Notes |
|-------|----------|---------|-------|
| `audio_encode_queue_` | 2 | PCM frames | Kecil karena C3 RAM terbatas |
| `audio_send_queue_` | 3 | Opus packets | Minimal untuk C3 |
| `audio_decode_queue_` | 10 | Opus packets | Buffer untuk playback |
| `audio_playback_queue_` | 1 | PCM frames | Single frame untuk C3 |

**Catatan:** Queue size lebih kecil dibanding ESP32 biasa karena RAM terbatas. Ini mempengaruhi buffering behavior dan menjadi variabel penelitian.

---

## 9. YouTube Audio Streaming

### 9.1 Alur Kerja

```
1. User bilang "Putar Despacito"
2. Xiaozhi AI → MCP tools/call play_youtube_song(video_id, title)
3. Backend simpan command ke audio_queue
4. ESP32 polling: GET /api/device/audio/commands?mac=XX
5. ESP32 terima command: {video_id: "kJQP7kiw5Fk", title: "Despacito"}
6. ESP32 ACK: POST /api/device/audio/ack
7. ESP32 report status: POST /api/device/audio/status {playing}
8. ESP32 evaluate bitrate dari RSSI + free SRAM
9. ESP32 buka HTTP stream: GET /api/audio/stream/kJQP7kiw5Fk?br=12k
10. Backend transcode YouTube → Opus → HTTP chunked
11. ESP32 demux Ogg container → raw Opus frames
12. ESP32 decode Opus → PCM → speaker (MAX98357A)
13. Monitor buffer health, downshift jika perlu
14. Selesai → report status: POST /api/device/audio/status {stopped}
```

### 9.2 Adaptive Bitrate Algorithm

```cpp
// Evaluasi awal berdasarkan RSSI dan free SRAM
wifi_ap_record_t ap_info;
int8_t rssi = ap_info.rssi;
size_t free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

std::string bitrate;
if (rssi >= -65 && free_sram >= 48000)      bitrate = "16k";
else if (rssi >= -75 && free_sram >= 36000)  bitrate = "12k";
else if (rssi >= -82 && free_sram >= 28000)  bitrate = "8k";
else                                         bitrate = "6k";
```

**Threshold Table:**

| Bitrate | Min RSSI | Min Free SRAM | Kualitas |
|---------|----------|---------------|----------|
| 16k | -65 dBm | 48 KB | Tinggi (WiFi bagus) |
| 12k | -75 dBm | 36 KB | Sedang (default) |
| 8k | -82 dBm | 28 KB | Rendah (WiFi lemah) |
| 6k | apapun | <28 KB | Minimum (survival) |

### 9.3 Buffer Starvation Detection

```cpp
// Monitor decode queue setiap frame
size_t q_size = app.GetAudioService().GetDecodeQueueSize();
if (frames_pushed > 20 && q_size == 0) {
    starvation_count++;
    if (starvation_count >= 5) {
        // Downshift bitrate
        if (current_bitrate == "16k") current_bitrate = "12k";
        else if (current_bitrate == "12k") current_bitrate = "8k";
        else if (current_bitrate == "8k") current_bitrate = "6k";
        // Reconnect ke stream dengan bitrate baru
    }
}
```

**Algoritma:**
1. Setelah 20 frame pertama (warmup), mulai monitor decode queue
2. Jika queue kosong 5x berturut-turut → buffer starvation detected
3. Downshift 1 level bitrate (16k→12k→8k→6k)
4. Reconnect ke stream dengan `?br=<new_bitrate>&start=<current_position>`
5. Jika queue pulih (≥3 frames), reset starvation counter

### 9.4 Frame Pacing

```cpp
// Pre-buffer 8 frames (480ms) lalu pace setiap 60ms
if (frame_idx > 8) {
    target_time += 0.060;  // 60ms per frame
    sleep_duration = target_time - time.monotonic();
    if (sleep_duration > 0)
        await asyncio.sleep(sleep_duration);
    else if (sleep_duration < -0.300)
        target_time = time.monotonic();  // Reset clock jika lag besar
}
```

---

## 10. MCP Server (On-Device)

### 10.1 Struktur Pesan MCP

```json
// Server → ESP32: Tool Call Request
{
    "session_id": "xxx",
    "type": "mcp",
    "payload": {
        "jsonrpc": "2.0",
        "method": "tools/call",
        "params": {
            "name": "self.audio.play_youtube",
            "arguments": {"video_id": "kJQP7kiw5Fk", "title": "Despacito"}
        },
        "id": 1
    }
}

// ESP32 → Server: Tool Call Response
{
    "session_id": "xxx",
    "type": "mcp",
    "payload": {
        "jsonrpc": "2.0",
        "id": 1,
        "result": {"content": [{"type": "text", "text": "true"}], "isError": false}
    }
}
```

### 10.2 MCP Methods

| Method | Pengirim | Fungsi |
|--------|----------|--------|
| `initialize` | Server→ESP32 | Handshake, capability exchange |
| `tools/list` | Server→ESP32 | Discover available tools |
| `tools/call` | Server→ESP32 | Execute a tool |

### 10.3 Daftar Tools

| Tool Name | Parameter | Fungsi |
|-----------|-----------|--------|
| `self.get_device_status` | - | Status volume, battery, WiFi, speaker |
| `self.audio_speaker.set_volume` | volume (0-100) | Atur volume speaker |
| `self.audio.play_youtube` | video_id, title | Putar YouTube audio |
| `self.screen.set_brightness` | brightness (0-100) | Atur kecerahan OLED |
| `self.screen.set_theme` | theme (light/dark) | Ganti tema display |
| `self.camera.take_photo` | question | Ambil foto + explain |
| `self.get_system_info` | - | System info ESP32 |
| `self.reboot` | - | Restart device |
| `self.upgrade_firmware` | url | OTA firmware update |

---

## 11. Sistem Operasi dan Task Management

### 11.1 State Machine

```
                    ┌──────────────┐
                    │   Starting   │
                    └──────┬───────┘
                           │
                    ┌──────▼───────┐
          ┌────────│    Idle       │────────┐
          │        └──────┬───────┘        │
          │               │                │
   ┌──────▼───────┐ ┌─────▼──────┐  ┌──────▼───────┐
   │ WiFi Config  │ │ Connecting │  │ YouTube      │
   │ (AP Mode)    │ │            │  │ Streaming    │
   └──────────────┘ └─────┬──────┘  └──────────────┘
                          │
                   ┌──────▼───────┐
          ┌────────│  Listening   │────────┐
          │        └──────┬───────┘        │
          │               │                │
   ┌──────▼───────┐ ┌─────▼──────┐  ┌──────▼───────┐
   │ Mode Menu    │ │ Speaking   │  │ Error        │
   │ (Hold 5s)    │ │ (TTS)      │  │ Recovery     │
   └──────────────┘ └────────────┘  └──────────────┘
```

### 11.2 Task Scheduling

FreeRTOS mengelola task dengan prioritas:

```
Priority 8: audio_input (mic capture, paling kritis)
Priority 4: audio_output (speaker playback)
Priority 2: opus_codec (encode/decode)
Priority 2: yt_poller (YouTube polling)
Priority 1: application (main loop, state management)
```

---

## 12. Adaptive Bitrate Streaming

### 12.1 Keputusan Desain

ESP32-C3 memiliki RAM ~400KB yang harus dibagi antara:
- WiFi stack (~50KB)
- Audio pipeline (~30KB)
- MCP/JSON parser (~10KB)
- Application logic (~20KB)
- Free heap target: ≥28KB untuk streaming stabil

Jika free heap turun di bawah threshold, sistem downshift bitrate untuk mencegah crash.

### 12.2 Metrics yang Bisa Diukur untuk Penelitian

| Metric | Cara Ukur | Lokasi Code |
|--------|-----------|-------------|
| Initial playback latency | Timestamp `PlaySong()` → frame pertama di-decode | `youtube_audio_poller.cc` |
| Buffer starvation events | Counter `starvation_count` | `StreamAudio()` |
| Free heap saat streaming | `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` | Sebelum/sesudah stream |
| RSSI saat streaming | `esp_wifi_sta_get_ap_info()` | Sebelum stream |
| Frame throughput | `frames_pushed / elapsed_time` | `StreamAudio()` |
| Reconnect count | Counter reconnect dalam streaming | `StreamAudio()` |
| Total bytes streamed | `total_bytes_streamed` | `StreamAudio()` |
| Bitrate shift events | Log setiap downshift | `StreamAudio()` |

---

## 13. Struktur File dan Direktori

```
xiaozhi-AI-esp32c3-mini-main/
├── CMakeLists.txt              # Build configuration
├── README.md                   # Project overview
├── partitions/                 # Flash partition tables
│   └── v2/                     # 4MB flash layout
├── main/                       # Source code utama
│   ├── CMakeLists.txt          # Main component build
│   ├── main.cc                 # Entry point
│   ├── application.cc/.h       # State machine
│   ├── mcp_server.cc/.h        # MCP server
│   ├── youtube_audio_poller.*  # YouTube streaming
│   ├── settings_manager.*      # NVS settings
│   ├── web_settings_server.*   # WiFi config portal
│   ├── sleep_manager.*         # Power management
│   ├── cellular_module.*       # Cellular support
│   ├── audio/                  # Audio subsystem
│   ├── boards/                 # Board definitions
│   ├── protocols/              # WebSocket/MQTT
│   ├── display/                # OLED/LVGL
│   ├── mode/                   # Dual-mode
│   ├── chronchi/               # Chronos BLE
│   └── assets/                 # Language config
├── components/                 # External components
│   └── 78__esp-wifi-connect/   # WiFi config AP
├── scripts/                    # Build/flash scripts
├── docs/                       # Documentation
│   ├── features/               # Feature docs
│   ├── modes/                  # Mode docs
│   ├── mcp-protocol.md         # MCP specification
│   ├── websocket.md            # WebSocket protocol
│   └── mqtt-udp.md             # MQTT alternative
├── build/                      # Build artifacts
│   └── xiaozhi.bin             # Compiled firmware
└── artifacts/                  # Release binaries
    └── production-4mb-2.1.0/
```

---

## 14. Konfigurasi Pin dan Hardware

### 14.1 Pin Map (esp32c3-inmp441/config.h)

| GPIO | Fungsi | Peripheral | Notes |
|------|--------|------------|-------|
| GPIO0 | OLED SDA | I2C | SSD1306 data |
| GPIO1 | Battery ADC | ADC1_CH1 | 100k/100k voltage divider |
| GPIO2 | Reset SSID Button | Digital input | Hold 5s → clear WiFi |
| GPIO3 | Boot/Chat Button | Digital input | Short=chat, Hold 5s=mode menu |
| GPIO4 | Mic Data In (DIN) | I2S | INMP441 SD |
| GPIO5 | I2S BCLK (shared) | I2S | INMP441 SCK + MAX98357A BCLK |
| GPIO6 | I2S WS/LRC (shared) | I2S | INMP441 WS + MAX98357A LRC |
| GPIO7 | Speaker Data Out (DOUT) | I2S | MAX98357A DIN |
| GPIO8 | Reserved | - | (rusak di board tertentu) |
| GPIO9 | Reserved | - | (rusak di board tertentu) |
| GPIO10 | OLED SCL | I2C | SSD1306 clock |

### 14.2 Battery Monitoring

```
BAT+ → 100kΩ → GPIO1 → 100kΩ → GND
                      │
                    100nF
                      │
                     GND

Voltage = ADC_reading × 2 × (3.3V / 4095)
```

---

## 15. Alur Kerja Lengkap

### 15.1 Boot Sequence

```
1. ESP32-C3 power on
2. NVS init → baca WiFi config, mode, server domain
3. WiFi connect → DHCP → DNS
4. Audio codec init (I2S, INMP441, MAX98357A)
5. Display init (SSD1306)
6. Wake word engine init
7. MCP server init → register tools
8. YouTube Audio Poller start
9. Mode check → Xiaozhi mode atau Chronos mode
10. Enter main loop (idle state)
```

### 15.2 AI Conversation Flow

```
1. User tekan tombol (GPIO3) atau wake word detected
2. OpenAudioChannel() → WebSocket connect ke wss://api.xiaozhi.me
3. Send hello message (features: mcp=true)
4. Wait server hello response (session_id)
5. Send listen/start → mulai record mic
6. Opus encode → send binary frames ke server
7. Server: STT → LLM → response
8. Server: send stt/text (display di OLED)
9. Server: send tts/start → kirim audio binary frames
10. ESP32: decode Opus → speaker playback
11. Server: send tts/stop → selesai
12. CloseAudioChannel() atau auto-listen lagi
```

### 15.3 YouTube Streaming Flow

```
1. User: "Putar Despacito"
2. Xiaozhi AI: MCP tools/call play_youtube_song
3. Backend: simpan ke audio_queue
4. ESP32: poll GET /commands?mac=XX → terima command
5. ESP32: POST /ack → POST /status {playing}
6. ESP32: evaluate bitrate dari RSSI + SRAM
7. ESP32: GET /stream/<vid>?br=12k → HTTP 200 chunked
8. Backend: FFmpeg transcode YouTube → Opus Ogg → HTTP chunks
9. ESP32: demux Ogg → raw Opus frames → decode → speaker
10. Monitor buffer health, downshift jika perlu
11. Selesai atau user abort → POST /status {stopped}
```

---

## 16. Daftar MCP Tools

### Common Tools (selalu tersedia)

| Tool | Description | Parameters |
|------|-------------|------------|
| `self.get_device_status` | Real-time device status (volume, battery, WiFi) | - |
| `self.audio_speaker.set_volume` | Set speaker volume | volume: int (0-100) |
| `self.audio.play_youtube` | Play YouTube audio stream | video_id: str, title: str |

### Display Tools (jika OLED ada)

| Tool | Description | Parameters |
|------|-------------|------------|
| `self.screen.set_brightness` | Set OLED brightness | brightness: int (0-100) |
| `self.screen.set_theme` | Set display theme | theme: "light"/"dark" |

### User-Only Tools

| Tool | Description | Parameters |
|------|-------------|------------|
| `self.get_system_info` | ESP32 system information | - |
| `self.reboot` | Restart device | - |
| `self.upgrade_firmware` | OTA firmware update | url: str |

---

## 17. Error Handling dan Recovery

### 17.1 WiFi Disconnect

```
WiFi disconnect detected →
  Attempt reconnect (3x) →
    Success: resume normal
    Fail: Enter WiFi config mode (AP hotspot)
```

### 17.2 WebSocket Disconnect

```
WebSocket close detected →
  CloseAudioChannel() →
    Return to Idle state →
      Next trigger: OpenAudioChannel() baru
```

### 17.3 YouTube Stream Error

```
HTTP stream error →
  Check frames_pushed > 0 →
    Yes: retry dengan bitrate lebih rendah (max 2x)
    No: give up, return to Idle
```

### 17.4 MCP Tool Error

```
Unknown tool → ReplyError(-32601, "Unknown tool: xxx")
Invalid params → ReplyError(-32602, "Invalid params")
Missing method → ReplyError(-32600, "Missing method")
```

---

## 18. Power Management

### 18.1 Audio Power Timer

```cpp
#define AUDIO_POWER_TIMEOUT_MS 30000  // 30 detik idle

// Saat idle 30 detik:
// - Mic dimatikan (hemat ~10mA)
// - Speaker dimatikan (hemat ~5mA)
// - Wake word engine tetap jalan (jika enabled)

// Saat aktif (listen/speak):
// - Mic dinyalakan
// - Speaker dinyalakan
```

### 18.2 Power Save Levels

| Level | Mode | WiFi | CPU | Penggunaan |
|-------|------|------|-----|------------|
| `PERFORMANCE` | Active | Full speed | 160MHz | YouTube streaming, AI chat |
| `LOW_POWER` | Idle | Power save | 80MHz | Standby, waiting |
| `DEEP_SLEEP` | Sleep | Off | Off | Battery conservation |

---

## 19. Troubleshooting

| Masalah | Cek | Solusi |
|---------|-----|--------|
| Tidak ada suara | MAX98357A VIN | Harus **5V**, bukan 3.3V |
| Noise di speaker | Power supply | Tambah 100µF capacitor VIN-GND |
| Mic tidak jalan | INMP441 L/R | Hubungkan ke **GND** |
| WiFi gagal connect | SSID/Password | Hold GPIO2 5s → reset WiFi |
| YouTube tidak mulai | Free heap | Cek `heap_caps_get_free_size()` > 28KB |
| Audio terputus-putus | RSSI + SRAM | Cek adaptive bitrate shift di log |
| OLED tidak tampil | I2C wiring | Cek GPIO0→SDA, GPIO10→SCL |
| MCP tool tidak jalan | WebSocket | Cek koneksi ke wss://api.xiaozhi.me |
| Boot loop | Flash size | Pastikan 4MB flash, partition table benar |

---

## Catatan untuk Penelitian

### Variabel yang Bisa Diukur

| Layer | Variabel | Alat Ukur |
|-------|----------|-----------|
| Layer 1 (AI Communication) | MCP tool invocation latency, WebSocket message RTT | ESP_LOGI timestamps |
| Layer 2 (Backend Networking) | Command delivery latency (poll→receive), HTTP stream connect time | ESP_LOGI timestamps |
| Layer 3 (IoT Media Delivery) | Initial playback latency, buffer starvation count, frame throughput, bitrate shift events, free heap during stream | Internal counters + heap monitor |

### Kontribusi ke Skripsi

Dokumentasi ini menunjukkan bahwa sistem bukan hanya "ESP32 memutar YouTube", tapi sebuah **distributed communication architecture** dengan:
- MCP over WebSocket (Layer 1)
- Device-aware command routing via HTTP polling (Layer 2)
- Adaptive bitrate HTTP chunked streaming (Layer 3)
- Semua berjalan di perangkat dengan RAM 400KB

---

*Dokumentasi ini dibuat berdasarkan analisis source code: `main/mcp_server.cc`, `main/youtube_audio_poller.cc`, `main/audio/audio_service.cc`, `main/protocols/websocket_protocol.cc`, `main/boards/esp32c3-inmp441/config.h`, `docs/websocket.md`, `docs/mcp-protocol.md`, `docs/features/audio-system.md`*
