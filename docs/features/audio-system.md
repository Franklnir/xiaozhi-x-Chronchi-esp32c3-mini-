# Feature: Audio System

---

## Overview

ESP32-C3 memiliki 1 I2S peripheral yang di-share antara mikrofon (INMP441) dan speaker (MAX98357A). Clock (BCLK, WS/LRC) di-share, data terpisah (DIN untuk mic, DOUT untuk speaker).

---

## Hardware

### I2S Configuration

| Pin | Function | Connected To |
|-----|----------|--------------|
| GPIO 5 | BCLK (Bit Clock) | INMP441 SCK + MAX98357A BCLK |
| GPIO 6 | WS (Word Select) | INMP441 WS + MAX98357A LRC |
| GPIO 4 | DIN (Data In) | INMP441 SD |
| GPIO 7 | DOUT (Data Out) | MAX98357A DIN |

### Sample Rate

```c
// boards/esp32c3-inmp441/config.h
#define AUDIO_INPUT_SAMPLE_RATE  24000  // Hz
#define AUDIO_OUTPUT_SAMPLE_RATE 24000  // Hz
```

**Catatan:** Server Xiaozhi default 16kHz. Resampling dilakukan otomatis di AudioService.

### INMP441 (Microphone)

- Type: I2S MEMS
- Direction: Mono (L/R → GND untuk left channel)
- Sample rate: 24kHz (configurable)
- Bit depth: 16-bit

### MAX98357A (Speaker Amplifier)

- Type: I2S Class D amplifier
- Power: 3W @ 4Ω
- Supply: 5V (JANGAN 3.3V!)
- Output: Speaker 3W 4Ω

**Wiring:**
```
5V ──────► VIN
GND ─────► GND
GPIO 7 ──► DIN
GPIO 5 ──► BCLK
GPIO 6 ──► LRC
```

**Troubleshooting:**
- Tidak ada suara? Cek VIN pakai 5V (bukan 3.3V)
- Noise? Tambah 100µF capacitor antara VIN dan GND
- Volume rendah? Cek `SetOutputVolume(100)` dipanggil

---

## Audio Codec Interface

```cpp
class AudioCodec {
public:
    virtual void Start();
    virtual void Stop();
    
    virtual int input_sample_rate() const;   // 24000
    virtual int output_sample_rate() const;  // 24000
    virtual int input_channels() const;      // 1 (mono)
    
    virtual bool input_enabled() const;
    virtual void EnableInput(bool enable);
    virtual bool output_enabled() const;
    virtual void EnableOutput(bool enable);
    
    virtual bool InputData(std::vector<int16_t>& data);
    virtual bool OutputData(std::vector<int16_t>& data);
    
    virtual void SetOutputVolume(int volume);  // 0-100
};
```

---

## Opus Codec

### Encoder (mic → server)

```cpp
OpusEncoderWrapper encoder(16000, 1, 60);  // 16kHz, mono, 60ms frames
encoder.SetComplexity(0);  // Lowest complexity, hemat CPU

// Encode PCM → Opus
encoder.Encode(pcm_data, opus_payload);
```

### Decoder (server → speaker)

```cpp
OpusDecoderWrapper decoder(output_sample_rate, 1, 60);

// Decode Opus → PCM
decoder.Decode(opus_payload, pcm_data);
```

### Resampling

Server kadang kirim 16kHz, speaker butuh 24kHz:

```cpp
OpusResampler resampler;
resampler.Configure(16000, 24000);
resampler.Process(input, input_size, output);
```

---

## AudioService Pipeline

### Data Flow

```
┌─────────────────────────────────────────────────────────┐
│                    AudioService                         │
│                                                         │
│  MIC PATH:                                              │
│  ┌────────┐   ┌──────────┐   ┌────────┐   ┌─────────┐ │
│  │ INMP441│──►│Resampler │──►│ Encoder│──►│SendQueue│ │
│  │ (I2S)  │   │24k→16k   │   │ (Opus) │   │         │ │
│  └────────┘   └──────────┘   └────────┘   └────┬────┘ │
│                                                 │      │
│                                          Protocol::Send│
│                                                 │      │
│  SPEAKER PATH:                                    │      │
│  ┌─────────┐   ┌────────┐   ┌──────────┐   ┌────┴────┐│
│  │MAX98357A│◄──│Resampler│◄──│ Decoder  │◄──│DecodeQ  ││
│  │ (I2S)   │   │16k→24k  │   │ (Opus)   │   │         ││
│  └─────────┘   └─────────┘   └──────────┘   └─────────┘│
│                                                         │
│  PLAYBACK PATH (local songs):                           │
│  ┌─────────┐   ┌──────────┐   ┌──────────────────────┐ │
│  │MAX98357A│◄──│Resampler │◄──│PlaySound(ogg_data)   │ │
│  │ (I2S)   │   │          │   │                      │ │
│  └─────────┘   └──────────┘   └──────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

### Tasks

| Task | Stack | Priority | Core | Fungsi |
|------|-------|----------|------|--------|
| `audio_input` | 6KB | 8 | 0 | Baca mic, proses audio |
| `audio_output` | 4KB | 4 | any | Tulis ke speaker |
| `opus_codec` | 26KB | 2 | any | Encode/decode Opus |

### Queues

| Queue | Max Size | Content |
|-------|----------|---------|
| `audio_encode_queue_` | 2 | PCM frames untuk di-encode |
| `audio_send_queue_` | 40 | Opus packets untuk dikirim ke server |
| `audio_decode_queue_` | 40 | Opus packets dari server untuk di-decode |
| `audio_playback_queue_` | 2 | PCM frames untuk speaker |
| `audio_testing_queue_` | 167 | Untuk audio testing mode |

### Key Methods

```cpp
// Push Opus packet dari server ke decode queue
bool PushPacketToDecodeQueue(std::unique_ptr<AudioStreamPacket> packet);

// Pop Opus packet untuk dikirim ke server
std::unique_ptr<AudioStreamPacket> PopPacketFromSendQueue();

// Play local sound (OGG data)
void PlaySound(const std::string_view& sound);

// Reset decoder (clear buffers)
void ResetDecoder();

// Enable/disable tasks
void EnableWakeWordDetection(bool enable);
void EnableVoiceProcessing(bool enable);
void EnableAudioTesting(bool enable);
void EnableLoopback(bool enable);
```

---

## Volume Control

```cpp
// Via MCP tool
codec->SetOutputVolume(100);  // 0-100

// Via voice command
// "Volume setengah" → self.audio_speaker.set_volume(50)
// "Volume penuh" → self.audio_speaker.set_volume(100)
```

**Xichi mode:** Volume SELALU 100%, di-set setiap kali TTS start.

---

## Power Management

```cpp
// Audio power timer: matikan mic/speaker saat idle
#define AUDIO_POWER_TIMEOUT_MS 30000  // 30 detik idle → matikan

// Saat idle:
// - Mic dimatikan (hemat power)
// - Speaker dimatikan (hemat power)
// - Wake word tetap jalan (kalau enabled)

// Saat aktif:
// - Mic dinyalakan
// - Speaker dinyalakan
```

---

## Troubleshooting

| Masalah | Cek | Solusi |
|---------|-----|--------|
| Tidak ada suara | MAX98357A VIN | Harus 5V, bukan 3.3V |
| Tidak ada suara | GPIO7 → DIN | Pastikan terhubung |
| Noise di speaker | Capacitor | Tambah 100µF VIN-GND |
| Volume rendah | SetOutputVolume | Pastikan 100 |
| Mic tidak jalan | INMP441 L/R | Hubungkan ke GND |
| Mic tidak jalan | GPIO4 → SD | Pastikan terhubung |
| Audio terputus | RAM | Cek free heap > 30KB |
| Distorsi | Sample rate | Cek resampling benar |
