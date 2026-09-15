# Feature: MCP Protocol (Model Context Protocol)

---

## Overview

MCP (Model Context Protocol) adalah protokol untuk mengontrol device ESP32 dari server Xiaozhi. Server bisa "melihat" status device dan "mengontrol" hardware lewat tool calls. Semua komunikasi MCP berjalan di atas WebSocket/MQTT.

---

## Protocol Format

### JSON-RPC 2.0

Semua MCP messages menggunakan JSON-RPC 2.0:

**Request (Server → Device):**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "tools/call",
    "params": {
        "name": "self.audio_speaker.set_volume",
        "arguments": {
            "volume": 80
        }
    }
}
```

**Response (Device → Server):**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": {
        "content": [
            { "type": "text", "text": "true" }
        ],
        "isError": false
    }
}
```

**Error Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "error": {
        "code": -1,
        "message": "Unknown tool: invalid.tool"
    }
}
```

### WebSocket Message Wrapper

```json
{
    "session_id": "xxx",
    "type": "mcp",
    "payload": { ... }  // JSON-RPC 2.0 di dalam
}
```

---

## Tool Registration

### AddTool API

```cpp
auto& mcp_server = McpServer::GetInstance();

mcp_server.AddTool(
    "self.tool_name",           // Nama tool
    "Deskripsi tool",           // Deskripsi untuk LLM
    PropertyList({              // Parameters
        Property("param1", kPropertyTypeString),
        Property("param2", kPropertyTypeInteger, min, max)
    }),
    [](const PropertyList& properties) -> ReturnValue {
        // Implementation
        auto param1 = properties["param1"].value<std::string>();
        auto param2 = properties["param2"].value<int>();
        return "result";
    }
);
```

### Tool Categories

| Category | Prefix | Contoh |
|----------|--------|--------|
| Device status | `self.get_*` | `self.get_device_status` |
| Audio | `self.audio_*` | `self.audio_speaker.set_volume` |
| Display | `self.screen.*` | `self.screen.set_brightness` |
| System | `self.*` | `self.reboot`, `self.upgrade_firmware` |
| Custom (Xichi) | `notification.*` | `notification.read` |

---

## Built-in Tools

### Common Tools (selalu tersedia)

#### `self.get_device_status`
Mendapatkan status device secara real-time.

```json
// Request
{"method": "tools/call", "params": {"name": "self.get_device_status"}}

// Response
{"result": {"content": [{"type": "text", "text": "{\"volume\":80,\"battery\":85,...}"}]}}
```

#### `self.audio_speaker.set_volume`
Mengatur volume speaker.

| Parameter | Type | Range | Required |
|-----------|------|-------|----------|
| `volume` | integer | 0-100 | ✅ |

```json
{"method": "tools/call", "params": {"name": "self.audio_speaker.set_volume", "arguments": {"volume": 80}}}
```

#### `self.screen.set_brightness`
Mengatur kecerahan layar.

| Parameter | Type | Range | Required |
|-----------|------|-------|----------|
| `brightness` | integer | 0-100 | ✅ |

#### `self.screen.set_theme`
Mengatur tema layar.

| Parameter | Type | Values | Required |
|-----------|------|--------|----------|
| `theme` | string | "light", "dark" | ✅ |

### User-Only Tools (hanya untuk user interaction)

#### `self.get_system_info`
Informasi sistem ESP32.

#### `self.reboot`
Restart device.

#### `self.upgrade_firmware`
Update firmware dari URL.

| Parameter | Type | Required |
|-----------|------|----------|
| `url` | string | ✅ |

#### `self.camera.take_photo`
Ambil foto (jika ada kamera).

| Parameter | Type | Required |
|-----------|------|----------|
| `question` | string | ✅ |

#### `self.screen.snapshot`
Screenshot OLED.

| Parameter | Type | Default | Required |
|-----------|------|---------|----------|
| `url` | string | - | ✅ |
| `quality` | integer | 80 | ❌ |

#### `self.screen.preview_image`
Tampilkan gambar di OLED.

| Parameter | Type | Required |
|-----------|------|----------|
| `url` | string | ✅ |

### Board-Specific Tools

#### `self.lamp.get_state`
Status lampu.

#### `self.lamp.turn_on`
Nyalakan lampu.

#### `self.lamp.turn_off`
Matikan lampu.

#### `self.set_press_to_talk`
Atur mode tombol.

---

## Xichi Mode: notification.read Tool

### Tool Definition

```json
{
    "name": "notification.read",
    "description": "Membacakan notifikasi yang masuk dari HP pengguna. Langsung bacakan isi notifikasi, jangan tambahkan basa-basi.",
    "parameters": {
        "type": "object",
        "properties": {
            "source_app": {
                "type": "string",
                "description": "Nama aplikasi pengirim (GoPay, WhatsApp, Gmail, dll)"
            },
            "category": {
                "type": "string",
                "description": "Kategori: PAYMENT, MESSAGE, EMAIL, ORDER, SYSTEM"
            },
            "primary_text": {
                "type": "string",
                "description": "Teks utama notifikasi"
            },
            "secondary_text": {
                "type": "string",
                "description": "Teks tambahan (nominal, pengirim, dll)"
            }
        },
        "required": ["source_app", "primary_text"]
    }
}
```

### Implementation

```python
def notification_read(source_app, category, primary_text, secondary_text=""):
    templates = {
        "PAYMENT": f"Notifikasi pembayaran dari {source_app}: {primary_text}",
        "MESSAGE": f"Pesan masuk dari {source_app}: {primary_text}",
        "EMAIL": f"Email masuk di {source_app}: {primary_text}",
        "ORDER": f"Update pesanan dari {source_app}: {primary_text}",
        "SYSTEM": f"Notifikasi sistem dari {source_app}: {primary_text}",
    }
    template = templates.get(category, f"Notifikasi dari {source_app}: {primary_text}")
    result = template
    if secondary_text:
        result += f", {secondary_text}"
    return result
```

### LLM System Prompt

```
Ketika menerima tool call notification.read:
- Langsung bacakan isi notifikasi dengan natural
- Gunakan bahasa Indonesia
- Jangan tanya "mau dibacakan?" atau "ada yang bisa saya bantu?"
- Setelah selesai membacakan, diam
- Format: "[tipe] dari [sumber]: [isi]"
- Contoh: "Ada pembayaran masuk dari GoPay sebesar lima puluh ribu rupiah"
```

---

## MCP Server Implementation

### Class Structure

```cpp
class McpServer {
public:
    static McpServer& GetInstance();
    
    void AddCommonTools();          // Device status, volume, dll
    void AddUserOnlyTools();        // System info, reboot, dll
    void AddTool(McpTool* tool);
    void AddTool(const std::string& name, const std::string& description,
                 const PropertyList& properties,
                 std::function<ReturnValue(const PropertyList&)> callback);
    
    void ParseMessage(const std::string& message);  // Parse JSON
    void DoToolCall(int id, const std::string& tool_name,
                    const cJSON* tool_arguments);     // Execute tool

private:
    std::vector<McpTool*> tools_;
};
```

### Message Flow

```
Server → Device: {"type":"mcp","payload":{...}}
  │
  ▼
Protocol::OnIncomingJson()
  │
  ▼
McpServer::ParseMessage()
  │
  ├─ method="tools/list" → GetToolsList()
  │    Return: list of available tools + descriptions
  │
  └─ method="tools/call" → DoToolCall()
       │
       ├─ Find tool by name
       ├─ Parse arguments
       ├─ Execute callback
       └─ Return result/error
```

---

## MCP Capabilities Declaration

Saat handshake, device mengiklankan MCP support:

```json
{
    "type": "hello",
    "features": {
        "mcp": true
    }
}
```

Server kemudian bisa mengirim `tools/list` untuk melihat semua tools yang tersedia, dan `tools/call` untuk mengeksekusi tool.

---

## Best Practices

### Tool Naming
- Prefix dengan namespace: `self.`, `notification.`, `lamp.`
- Gunakan snake_case: `set_volume`, `get_state`
- Deskriptif: `self.audio_speaker.set_volume` bukan `self.vol`

### Parameter Validation
- Selalu cek required parameters
- Clamp integer values ke range yang valid
- Handle missing optional parameters dengan default

### Error Handling
- Return error message yang jelas
- Jangan crash pada invalid input
- Log errors untuk debugging

### Performance
- Tool execution di main thread (via Schedule)
- Jangan blocking call di tool callback
- Return result secepat mungkin
