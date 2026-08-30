# Roadmap: xiaozhi-AI-esp32c3-mini → Gen2P Feature Parity

## Status Legend
- 🔴 Not Started
- 🟡 In Progress
- 🟢 Completed
- ⚪ Not Applicable (hardware limitation)

---

## Phase 1: Core Infrastructure (Week 1-2)

### 1.1 Partition Table Update 🟡
- [ ] Add `assets` partition for network-loadable content
- [ ] Support 4MB flash layout (ESP32-C3 limitation)
- [ ] Implement SPIFFS filesystem for assets
- [ ] Add checksum validation for assets

### 1.2 OTA Enhancement 🟡
- [ ] Support differential OTA updates
- [ ] Add rollback mechanism on failed update
- [ ] Implement asset OTA (separate from firmware)
- [ ] Add progress tracking for downloads

### 1.3 Settings & Configuration 🟡
- [ ] Expand NVS storage for new features
- [ ] Add runtime configuration via web interface
- [ ] Implement configuration export/import
- [ ] Add factory reset with options (keep WiFi, keep settings, full reset)

---

## Phase 2: Networking (Week 3-4)

### 2.1 4G Cellular Support 🔴
- [ ] Add ML307R module driver
- [ ] Add EC801E module driver
- [ ] Add NT26 module driver
- [ ] Implement AT command parser
- [ ] Add SIM card management
- [ ] Implement network failover (WiFi ↔ 4G)
- [ ] Add signal strength indicator
- [ ] Add data usage tracking

### 2.2 Wired Ethernet 🔴
- [ ] Add W5500/ENC28J60 SPI Ethernet driver
- [ ] Implement DHCP client
- [ ] Add link status detection
- [ ] Implement network priority (Ethernet > WiFi > 4G)

### 2.3 USB RNDIS 🔴
- [ ] Add USB RNDIS class driver
- [ ] Implement USB network interface
- [ ] Add USB network detection

### 2.4 BluFi Provisioning 🔴
- [ ] Enable BT/BLE in sdkconfig
- [ ] Implement BluFi protocol
- [ ] Add BluFi pairing flow
- [ ] Add fallback to hotspot if BluFi fails

---

## Phase 3: Display & UI (Week 5-6)

### 3.1 LCD/TFT Display Support 🔴
- [ ] Add SPI LCD driver (ST7789, ILI9341, etc.)
- [ ] Add I2C LCD driver (SH1106, etc.)
- [ ] Implement display abstraction layer
- [ ] Add resolution-independent rendering
- [ ] Implement display rotation support

### 3.2 Battery & Power Management 🔴
- [ ] Add ADC battery voltage reading
- [ ] Implement battery percentage calculation
- [ ] Add charging detection (if supported)
- [ ] Implement low battery warning
- [ ] Add power saving modes
- [ ] Implement auto-shutdown on critical battery

### 3.3 Enhanced UI 🔴
- [ ] Add status bar (WiFi/4G signal, battery, time)
- [ ] Implement emoji rendering
- [ ] Add animation support
- [ ] Implement theme system
- [ ] Add font customization

---

## Phase 4: Audio & Voice (Week 7-8)

### 4.1 Full-Duplex AEC 🔴
- [ ] Implement device-side AEC
- [ ] Add server-side AEC support
- [ ] Implement echo cancellation tuning
- [ ] Add noise suppression
- [ ] Implement automatic gain control

### 4.2 Speaker Recognition 🔴
- [ ] Integrate 3D Speaker model
- [ ] Implement speaker enrollment
- [ ] Add speaker identification
- [ ] Implement per-user preferences

### 4.3 Enhanced Wake Word 🔴
- [ ] Support custom wake word training
- [ ] Add multiple wake word support
- [ ] Implement wake word sensitivity adjustment
- [ ] Add wake word feedback (LED, sound)

### 4.4 Realtime Voice Model 🔴
- [ ] Implement end-to-end voice streaming
- [ ] Add realtime ASR support
- [ ] Implement realtime TTS
- [ ] Add voice cloning support

---

## Phase 5: Vision & Camera (Week 9-10)

### 5.1 Camera Support 🔴
- [ ] Add OV2640/OV5640 camera driver
- [ ] Implement image capture
- [ ] Add video streaming support
- [ ] Implement image preprocessing

### 5.2 Vision AI Integration 🔴
- [ ] Add image classification
- [ ] Implement object detection
- [ ] Add OCR support
- [ ] Implement face detection/recognition

### 5.3 Vision MCP Tools 🔴
- [ ] Add camera.capture tool
- [ ] Add vision.analyze tool
- [ ] Add vision.detect tool
- [ ] Add vision.ocr tool

---

## Phase 6: MCP & Cloud Integration (Week 11-12)

### 6.1 Cloud-Side MCP 🔴
- [ ] Implement MCP client for cloud services
- [ ] Add smart home control (HomeAssistant)
- [ ] Add PC desktop control
- [ ] Add knowledge search integration
- [ ] Add email integration
- [ ] Add calendar integration

### 6.2 Device-Side MCP Enhancement 🟡
- [ ] Add GPIO control tools
- [ ] Add servo control tools
- [ ] Add LED control tools
- [ ] Add sensor reading tools
- [ ] Add relay control tools

### 6.3 MCP Protocol Enhancement 🟡
- [ ] Add MCP over MQTT transport
- [ ] Implement MCP tool discovery
- [ ] Add MCP tool permissions
- [ ] Implement MCP tool caching

---

## Phase 7: Assets & Customization (Week 13-14)

### 7.1 Custom Assets System 🔴
- [ ] Implement assets partition management
- [ ] Add wake word model loading from assets
- [ ] Add font loading from assets
- [ ] Add emoji pack loading from assets
- [ ] Add background image loading from assets
- [ ] Add sound effect loading from assets

### 7.2 Web-Based Assets Editor 🔴
- [ ] Create web interface for assets management
- [ ] Add wake word training interface
- [ ] Add font preview and selection
- [ ] Add emoji pack creator
- [ ] Add background image uploader
- [ ] Add sound effect uploader

### 7.3 Online Assets Repository 🔴
- [ ] Create assets repository structure
- [ ] Implement assets download manager
- [ ] Add version management for assets
- [ ] Implement assets sharing

---

## Phase 8: Board Support (Week 15-16)

### 8.1 New Board Support 🔴
- [ ] Add ESP32-S3 boards
- [ ] Add ESP32-C6 boards
- [ ] Add ESP32-P4 boards
- [ ] Add M5Stack boards
- [ ] Add Waveshare boards
- [ ] Add LILYGO boards

### 8.2 Board Abstraction 🟡
- [ ] Improve board abstraction layer
- [ ] Add runtime board detection
- [ ] Implement board-specific optimizations
- [ ] Add board configuration UI

---

## Phase 9: Protocol Enhancement (Week 17-18)

### 9.1 WebSocket Protocol v2 🔴
- [ ] Add binary protocol v2 support
- [ ] Implement message compression
- [ ] Add message encryption
- [ ] Implement reconnection improvements

### 9.2 MQTT Protocol Enhancement 🟡
- [ ] Add MQTT v5 support
- [ ] Implement MQTT over WebSocket
- [ ] Add MQTT message queuing
- [ ] Implement MQTT retained messages

### 9.3 UDP Protocol 🔴
- [ ] Implement UDP audio streaming
- [ ] Add UDP hole punching
- [ ] Implement UDP fallback
- [ ] Add UDP encryption

---

## Phase 10: Testing & Documentation (Week 19-20)

### 10.1 Testing 🔴
- [ ] Unit tests for new features
- [ ] Integration tests
- [ ] Hardware-in-the-loop tests
- [ ] Performance benchmarks
- [ ] Power consumption tests

### 10.2 Documentation 🔴
- [ ] Update README with new features
- [ ] Add hardware setup guides
- [ ] Add configuration guides
- [ ] Add troubleshooting guides
- [ ] Add API documentation

---

## Hardware Requirements for Full Gen2P

### Minimum (Current)
- ESP32-C3 (4MB flash)
- INMP441 microphone
- MAX98357A amplifier
- Speaker
- Button

### Recommended for Full Features
- ESP32-S3 (8MB/16MB flash) - for camera, more memory
- OV2640 camera module
- LCD/TFT display (ST7789, ILI9341)
- Battery + charging circuit
- 4G module (ML307R) - optional
- Ethernet module (W5500) - optional

---

## Priority Matrix

| Feature | Impact | Effort | Priority |
|---------|--------|--------|----------|
| Partition + Assets | High | Medium | P0 |
| 4G Support | High | High | P1 |
| LCD/TFT Display | Medium | Medium | P1 |
| Battery Management | Medium | Medium | P1 |
| Full-Duplex AEC | High | High | P2 |
| Camera/Vision | Medium | High | P2 |
| Cloud MCP | Medium | Medium | P2 |
| Speaker Recognition | Low | High | P3 |
| BluFi Provisioning | Low | Medium | P3 |
| Custom Assets Web | Low | Medium | P3 |

---

## Notes

1. **ESP32-C3 Limitation**: 4MB flash limits some features. Consider ESP32-S3 for full feature set.
2. **Memory Constraints**: Camera and vision features require more RAM than C3 has.
3. **Power Consumption**: 4G and camera significantly increase power consumption.
4. **Cost**: Full feature set increases hardware cost from ~$6 to ~$20-30.

---

## Next Steps

1. Start with Phase 1 (Partition + Assets) as foundation
2. Add 4G support if hardware available
3. Add LCD/TFT display for better UI
4. Add battery management for portable use
5. Iterate on remaining features based on user feedback
