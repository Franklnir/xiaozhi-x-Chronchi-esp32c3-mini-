# Mode: Xiaozhi (Voice AI Assistant)

**BootMode enum:** `BootMode::Xiaozhi = 0`  
**Default mode:** Ya (jika tidak ada mode tersimpan)

---

## Deskripsi

Xiaozhi adalah mode utama — voice AI assistant yang memungkinkan user berbicara dengan AI lewat mikrofon dan mendengar jawaban lewat speaker. Menggunakan server Xiaozhi cloud untuk ASR (speech-to-text), LLM (large language model), dan TTS (text-to-speech).

---

## Fitur

### 1. Voice Chat (Ngobrol dengan AI)

**Flow:**
```
User bicara → INMP441 mic → Opus encode → WebSocket → Server
Server: ASR → LLM → TTS → Opus audio
Server → ESP32 → Opus decode → MAX98357A speaker → User dengar
```

**Dua mode input:**
- **Wake Word:** Katakan "Hi Jason" untuk aktivasi (hands-free)
- **Push-to-Talk:** Tekan tombol GPIO3, bicara, lepas untuk kirim

**Mode listening:**
| Mode | Aktivasi | Behavior |
|------|----------|----------|
| `kListeningModeAutoStop` | Wake word / auto | Otomatis stop setelah diam beberapa detik |
| `kListeningModeManualStop` | Push-to-talk (tahan) | Stop saat tombol dilepas |
| `kListeningModeRealtime` | Butuh AEC | Full-duplex, mic+speaker bareng |

### 2. Wake Word Detection

**Engine:** ESP-SR (ESP Speech Recognition)  
**Wake word:** "Hi Jason" (custom, bisa diubah)  
**Behavior:**
- Mendengarkan terus-menerus di background
- Saat terdeteksi → mulai recording → kirim ke server
- Ada debounce 2 detik (tidak bisa trigger berulang cepat)
- Ada cooldown setelah TTS selesai (hindari deteksi diri sendiri)

**Konfigurasi:**
```c
// boards/esp32c3-inmp441/config.h
#define HANDS_FREE_AUTO_LISTEN 1              // Auto-listen setelah boot
#define HANDS_FREE_AUTO_LISTEN_INTERVAL_MS 500 // Interval check
#define HANDS_FREE_IDLE_TIMEOUT_MS 30000       // Timeout idle → low power
```

### 3. Push-to-Talk

**Tombol:** GPIO3 (BOOT button)  
**Behavior:**
- Tekan & tahan → mulai recording
- Lepas → stop recording, kirim ke server
- Bisa interrupt TTS yang sedang jalan (AbortSpeaking)
- Bisa interrupt local song playback

### 4. MCP Device Control

Server Xiaozhi bisa mengontrol ESP32 lewat MCP tools:

| Tool | Fungsi |
|------|--------|
| `self.get_device_status` | Info volume, baterai, jaringan |
| `self.audio_speaker.set_volume` | Atur volume speaker (0-100) |
| `self.screen.set_brightness` | Atur kecerahan OLED |
| `self.screen.set_theme` | Ganti tema (light/dark) |
| `self.get_system_info` | Info sistem ESP32 |
| `self.reboot` | Restart device |
| `self.upgrade_firmware` | Update firmware dari URL |
| `self.camera.take_photo` | Ambil foto (jika ada kamera) |
| `self.screen.snapshot` | Screenshot OLED |
| `self.screen.preview_image` | Tampilkan gambar di OLED |
| `self.set_press_to_talk` | Atur mode tombol |
| `self.lamp.*` | Kontrol lampu (jika ada hardware) |

**Contoh voice command:**
- "Volume setengah" → `self.audio_speaker.set_volume(50)`
- "Layar lebih terang" → `self.screen.set_brightness(80)`
- "Restart dong" → `self.reboot()`
- "Baterai berapa?" → `self.get_device_status()`

### 5. Local Song Playback

**Trigger voice command:** "putar lagu", "nyanyi", "play song", "puterin lagu"  
**Stop command:** "stop", "berhenti", "hentikan", "pause", "cukup"

| Song | File | Trigger |
|------|------|---------|
| Song 1 | `/spiffs/song1.ogg` | "putar lagu 1" / "nyanyi satu" |
| Song 2 | `/spiffs/song2.ogg` | "putar lagu 2" / "nyanyi dua" |
| Song 3 | `/spiffs/song3.ogg` | "putar lagu 3" / "nyanyi tiga" |

**Storage:** SPIFFS partition (768 KB) atau embedded assets

### 6. WiFi Configuration

**Setup mode:**
1. Boot → klik tombol dalam 5 detik
2. Hotspot "Xiaozhi-XXXX" muncul
3. Connect HP → buka http://192.168.4.1
4. Masukkan WiFi credentials
5. Device restart → connect ke WiFi

**Voice reset:** "reset wifi", "ganti wifi", "hapus wifi"  
**Konfirmasi:** "ya" / "iya" dalam 15 detik

### 7. Firmware Upgrade

**OTA via server:** Device check versi baru saat boot  
**OTA via voice:** "upgrade firmware" → server kirim URL  
**Manual:** Flash via USB dengan `idf.py flash`

### 8. Server Connection

**Protocol:** WebSocket (default) atau MQTT  
**Server:** xiaozhi.me (official) atau self-hosted  
**Auth:** Bearer token di header  
**Hello handshake:** Device kirim audio params, server balas session_id

**Message types (Device → Server):**
```
{"type":"hello", ...}                    // Handshake
{"type":"listen","state":"start", ...}   // Mulai recording
{"type":"listen","state":"stop"}         // Stop recording
{"type":"listen","state":"detect", ...}  // Wake word terdeteksi
{"type":"abort", ...}                    // Interrupt TTS
{"type":"mcp", ...}                      // MCP tool call/result
[binary Opus frames]                     // Audio data
```

**Message types (Server → Device):**
```
{"type":"hello", ...}                    // Handshake ack
{"type":"stt","text":"..."}              // ASR result
{"type":"tts","state":"start"}           // TTS mulai
{"type":"tts","state":"sentence_start","text":"..."} // Kalimat
{"type":"tts","state":"stop"}            // TTS selesai
{"type":"mcp", ...}                      // MCP tool call
[binary Opus frames]                     // TTS audio data
```

---

## State Machine

```
┌──────────┐
│ ACTIVATING │ Boot, init hardware, check firmware
└─────┬──────┘
      ▼
┌──────────┐  Wake word / PTT    ┌───────────┐
│   IDLE   │ ──────────────────► │ LISTENING │
│ (standby)│ ◄────────────────── │(recording)│
└─────┬────┘  TTS stop / timeout └─────┬─────┘
      │                                │ VAD detect silence
      │ TTS start                      ▼
      │                          ┌───────────┐
      └─────────────────────────►│ SPEAKING  │
                                 │(TTS play) │
                                 └───────────┘
```

---

## Audio Pipeline

```
MIC (INMP441)
  │ I2S @ 24000 Hz, mono
  ▼
AudioService::AudioInputTask()
  │ Resample 24kHz → 16kHz (jika perlu)
  │ Audio processors (AEC, noise reduction)
  ▼
Opus Encoder (complexity=0)
  │ 60ms frames, 16kHz mono
  ▼
Send Queue → Protocol::SendAudio()
  │ WebSocket binary
  ▼
Server (ASR + LLM + TTS)
  │
  ▼
WebSocket binary → Opus frames
  │
  ▼
AudioService::OpusCodecTask()
  │ Opus decoder
  ▼
AudioService::AudioOutputTask()
  │ Resample (jika perlu)
  │ I2S output @ 24000 Hz
  ▼
Speaker (MAX98357A)
```

---

## OLED Display

**Idle:** Emoji face + status bar (WiFi, baterai, jam)  
**Listening:** Emoji listening + "Mendengarkan..."  
**Speaking:** Emoji speaking + teks response dari AI  
**Error:** Emoji error + pesan error

---

## Konfigurasi Kconfig

```
CONFIG_BOARD_TYPE_ESP32C3_INMP441=y
CONFIG_ENABLE_CHRONCHI_MODE=y          # Untuk mode selection
CONFIG_USE_ESP_WAKE_WORD=y             # Wake word detection
CONFIG_AUDIO_INPUT_SAMPLE_RATE=24000
CONFIG_AUDIO_OUTPUT_SAMPLE_RATE=24000
```

---

## Troubleshooting

| Masalah | Solusi |
|---------|--------|
| Tidak ada suara | Cek MAX98357A pakai 5V, GPIO7 → DIN |
| Mic tidak jalan | Cek INMP441 L/R → GND, GPIO4 → SD |
| WiFi tidak connect | Reset: tahan GPIO2 3 detik |
| Server error | Cek token, cek internet, cek server status |
| Wake word tidak deteksi | Pastikan assets partition ada model wake word |
| RAM rendah | Matikan wake word, kurangi WiFi buffer |
