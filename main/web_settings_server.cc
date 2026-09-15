#include "web_settings_server.h"
#include "settings_manager.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <cJSON.h>
#include <cstring>

#define TAG "WebSettings"

// Embedded HTML page (minified)
static const char MAIN_PAGE_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Xiaozhi AI Settings</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,-apple-system,sans-serif;background:#f5f5f5;padding:20px}
.container{max-width:600px;margin:0 auto}
h1{text-align:center;color:#333;margin-bottom:20px}
.card{background:#fff;border-radius:12px;padding:20px;margin-bottom:16px;box-shadow:0 2px 8px rgba(0,0,0,0.1)}
h2{color:#555;font-size:16px;margin-bottom:12px;border-bottom:1px solid #eee;padding-bottom:8px}
label{display:block;color:#666;font-size:14px;margin-bottom:4px}
input,select{width:100%;padding:10px;border:1px solid #ddd;border-radius:6px;font-size:14px;margin-bottom:12px}
input[type="checkbox"]{width:auto;margin-right:8px}
.btn{display:block;width:100%;padding:12px;border:none;border-radius:6px;font-size:16px;cursor:pointer;margin-bottom:8px}
.btn-primary{background:#4CAF50;color:#fff}
.btn-danger{background:#f44336;color:#fff}
.btn-secondary{background:#2196F3;color:#fff}
.btn:hover{opacity:0.9}
.wifi-list{list-style:none;padding:0}
.wifi-item{display:flex;justify-content:space-between;align-items:center;padding:10px;border-bottom:1px solid #eee}
.wifi-item:last-child{border-bottom:none}
.signal{color:#4CAF50}
.status{text-align:center;padding:10px;border-radius:6px;margin-bottom:12px}
.status-ok{background:#E8F5E9;color:#2E7D32}
.status-err{background:#FFEBEE;color:#C62828}
.hidden{display:none}
</style>
</head>
<body>
<div class="container">
<h1>🤖 Xiaozhi AI</h1>

<div class="card">
<h2>📶 WiFi Connection</h2>
<div id="wifi-status" class="status status-ok">Connected</div>
<ul id="wifi-list" class="wifi-list"></ul>
<button class="btn btn-secondary" onclick="scanWifi()">Scan Networks</button>
</div>

<div class="card">
<h2>⚙️ Basic Settings</h2>
<label>Wake Word</label>
<select id="wake_word">
<option value="hiesp">Hi ESP</option>
<option value="hijason">Hi Jason</option>
<option value="hilexin">Hi Lexin</option>
<option value="nihaoxiaozhi">Ni Hao Xiaozhi</option>
</select>

<label>OLED Mode</label>
<select id="oled_mode">
<option value="classic">Classic</option>
<option value="amoled">Animated</option>
</select>

<label>Language</label>
<select id="language">
<option value="id_ID">Indonesian</option>
<option value="en_US">English</option>
<option value="zh_CN">Chinese</option>
<option value="ja_JP">Japanese</option>
<option value="ko_KR">Korean</option>
</select>

<label>Volume (0-100)</label>
<input type="range" id="volume" min="0" max="100" value="50">
<span id="volume-val">50</span>
</div>

<div class="card">
<h2>🔧 Advanced Settings</h2>
<label>Custom OTA URL</label>
<input type="text" id="ota_url" placeholder="https://api.tenclass.net/xiaozhi/ota/">

<label><input type="checkbox" id="sleep_mode"> Enable Sleep Mode</label>
<label>Sleep Timeout (seconds)</label>
<input type="number" id="sleep_timeout" value="60" min="10" max="600">

<label><input type="checkbox" id="remember_bssid"> Remember BSSID</label>
<label>WiFi Max TX Power (dBm)</label>
<input type="number" id="tx_power" value="20" min="8" max="20">
</div>

<div class="card">
<h2>💾 Actions</h2>
<button class="btn btn-primary" onclick="saveSettings()">Save Settings</button>
<button class="btn btn-secondary" onclick="exportSettings()">Export Settings</button>
<button class="btn btn-secondary" onclick="importSettings()">Import Settings</button>
<button class="btn btn-danger" onclick="resetWifi()">Reset WiFi</button>
<button class="btn btn-danger" onclick="reboot()">Reboot Device</button>
</div>
</div>

<script>
const API='/api';
let settings={};

async function loadSettings(){
try{
const r=await fetch(API+'/config');
settings=await r.json();
document.getElementById('wake_word').value=settings.wake_word||'hilexin';
document.getElementById('oled_mode').value=settings.oled_mode||'classic';
document.getElementById('language').value=settings.language||'id_ID';
document.getElementById('volume').value=settings.volume||50;
document.getElementById('volume-val').textContent=settings.volume||50;
document.getElementById('ota_url').value=settings.ota_url||'';
document.getElementById('sleep_mode').checked=settings.sleep_mode||false;
document.getElementById('sleep_timeout').value=settings.sleep_timeout||60;
document.getElementById('remember_bssid').checked=settings.remember_bssid!==false;
document.getElementById('tx_power').value=settings.tx_power||20;
updateWifiList();
}catch(e){console.error('Load failed:',e)}
}

function updateWifiList(){
const list=document.getElementById('wifi-list');
list.innerHTML='';
(settings.wifi||[]).forEach((w,i)=>{
const li=document.createElement('li');
li.className='wifi-item';
li.innerHTML=`<span>${w.ssid}</span><button onclick="removeWifi('${w.ssid}')">❌</button>`;
list.appendChild(li);
});
}

async function scanWifi(){
const r=await fetch(API+'/scan');
const nets=await r.json();
const list=document.getElementById('wifi-list');
list.innerHTML='';
nets.forEach(n=>{
const li=document.createElement('li');
li.className='wifi-item';
li.innerHTML=`<span>${n.ssid} <span class="signal">${n.rssi}dBm</span></span><button onclick="connectWifi('${n.ssid}')">Connect</button>`;
list.appendChild(li);
});
}

async function connectWifi(ssid){
const pass=prompt('Password for '+ssid+':');
if(!pass)return;
await fetch(API+'/connect',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,pass})});
alert('Connecting to '+ssid+'...');
}

async function removeWifi(ssid){
await fetch(API+'/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'remove_wifi',ssid})});
loadSettings();
}

async function saveSettings(){
const data={
action:'save',
wake_word:document.getElementById('wake_word').value,
oled_mode:document.getElementById('oled_mode').value,
language:document.getElementById('language').value,
volume:parseInt(document.getElementById('volume').value),
ota_url:document.getElementById('ota_url').value,
sleep_mode:document.getElementById('sleep_mode').checked,
sleep_timeout:parseInt(document.getElementById('sleep_timeout').value),
remember_bssid:document.getElementById('remember_bssid').checked,
tx_power:parseInt(document.getElementById('tx_power').value)
};
await fetch(API+'/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)});
alert('Settings saved!');
}

async function resetWifi(){
if(!confirm('Reset all WiFi settings?'))return;
await fetch(API+'/reset-wifi',{method:'POST'});
alert('WiFi reset. Device will restart in AP mode.');
}

async function reboot(){
if(!confirm('Reboot device?'))return;
await fetch(API+'/reboot',{method:'POST'});
}

async function exportSettings(){
const r=await fetch(API+'/export');
const blob=await r.blob();
const a=document.createElement('a');
a.href=URL.createObjectURL(blob);
a.download='xiaozhi-settings.json';
a.click();
}

async function importSettings(){
const input=document.createElement('input');
input.type='file';
input.accept='.json';
input.onchange=async(e)=>{
const file=e.target.files[0];
const text=await file.text();
await fetch(API+'/import',{method:'POST',headers:{'Content-Type':'application/json'},body:text});
alert('Settings imported!');
loadSettings();
};
input.click();
}

document.getElementById('volume').oninput=function(){
document.getElementById('volume-val').textContent=this.value;
};

loadSettings();
</script>
</body>
</html>
)rawliteral";

WebSettingsServer::WebSettingsServer() {}
WebSettingsServer::~WebSettingsServer() { Stop(); }

bool WebSettingsServer::Start(uint16_t port) {
    if (server_) return true;
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.max_uri_handlers = 15;
    
    if (httpd_start(&server_, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return false;
    }
    
    // Register URI handlers
    httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = HandleRoot };
    httpd_uri_t get_config = { .uri = "/api/config", .method = HTTP_GET, .handler = HandleGetConfig };
    httpd_uri_t set_config = { .uri = "/api/config", .method = HTTP_POST, .handler = HandleSetConfig };
    httpd_uri_t scan = { .uri = "/api/scan", .method = HTTP_GET, .handler = HandleScanWifi };
    httpd_uri_t connect = { .uri = "/api/connect", .method = HTTP_POST, .handler = HandleConnectWifi };
    httpd_uri_t reset = { .uri = "/api/reset-wifi", .method = HTTP_POST, .handler = HandleResetWifi };
    httpd_uri_t export_s = { .uri = "/api/export", .method = HTTP_GET, .handler = HandleExportSettings };
    httpd_uri_t import_s = { .uri = "/api/import", .method = HTTP_POST, .handler = HandleImportSettings };
    httpd_uri_t reboot = { .uri = "/api/reboot", .method = HTTP_POST, .handler = HandleReboot };
    
    httpd_register_uri_handler(server_, &root);
    httpd_register_uri_handler(server_, &get_config);
    httpd_register_uri_handler(server_, &set_config);
    httpd_register_uri_handler(server_, &scan);
    httpd_register_uri_handler(server_, &connect);
    httpd_register_uri_handler(server_, &reset);
    httpd_register_uri_handler(server_, &export_s);
    httpd_register_uri_handler(server_, &import_s);
    httpd_register_uri_handler(server_, &reboot);
    
    ESP_LOGI(TAG, "Web settings server started on port %d", port);
    return true;
}

void WebSettingsServer::Stop() {
    if (server_) {
        httpd_stop(server_);
        server_ = nullptr;
    }
}

esp_err_t WebSettingsServer::HandleRoot(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, MAIN_PAGE_HTML, strlen(MAIN_PAGE_HTML));
}

esp_err_t WebSettingsServer::HandleGetConfig(httpd_req_t* req) {
    auto& settings = SettingsManager::GetInstance();
    std::string json = settings.ExportToJson();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.length());
}

esp_err_t WebSettingsServer::HandleSetConfig(httpd_req_t* req) {
    char buf[1024];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';
    
    cJSON* root = cJSON_Parse(buf);
    if (!root) return ESP_FAIL;
    
    auto& settings = SettingsManager::GetInstance();
    
    cJSON* action = cJSON_GetObjectItem(root, "action");
    if (cJSON_IsString(action) && strcmp(action->valuestring, "save") == 0) {
        cJSON* item;
        
        item = cJSON_GetObjectItem(root, "wake_word");
        if (cJSON_IsString(item)) settings.SetWakeWord(item->valuestring);
        
        item = cJSON_GetObjectItem(root, "oled_mode");
        if (cJSON_IsString(item)) settings.SetOledMode(item->valuestring);
        
        item = cJSON_GetObjectItem(root, "language");
        if (cJSON_IsString(item)) settings.SetLanguage(item->valuestring);
        
        item = cJSON_GetObjectItem(root, "volume");
        if (cJSON_IsNumber(item)) settings.SetVolume(item->valueint);
        
        item = cJSON_GetObjectItem(root, "ota_url");
        if (cJSON_IsString(item)) settings.SetOtaUrl(item->valuestring);
        
        item = cJSON_GetObjectItem(root, "sleep_mode");
        if (cJSON_IsBool(item)) settings.SetSleepModeEnabled(cJSON_IsTrue(item));
        
        item = cJSON_GetObjectItem(root, "sleep_timeout");
        if (cJSON_IsNumber(item)) settings.SetSleepTimeout(item->valueint);
        
        item = cJSON_GetObjectItem(root, "remember_bssid");
        if (cJSON_IsBool(item)) {
            settings.SetWifiRememberBssid(cJSON_IsTrue(item));
        }
        
        item = cJSON_GetObjectItem(root, "tx_power");
        if (cJSON_IsNumber(item)) settings.SetWifiMaxTxPower(item->valueint);
        
        httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    } else if (cJSON_IsString(action) && strcmp(action->valuestring, "remove_wifi") == 0) {
        cJSON* ssid = cJSON_GetObjectItem(root, "ssid");
        if (cJSON_IsString(ssid)) {
            settings.RemoveWifiCredential(ssid->valuestring);
            httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
        }
    }
    
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t WebSettingsServer::HandleScanWifi(httpd_req_t* req) {
    wifi_scan_config_t scan_config = {
        .show_hidden = true
    };
    
    esp_wifi_scan_start(&scan_config, true);
    
    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);
    
    wifi_ap_record_t* records = (wifi_ap_record_t*)malloc(count * sizeof(wifi_ap_record_t));
    esp_wifi_scan_get_ap_records(&count, records);
    
    cJSON* array = cJSON_CreateArray();
    for (int i = 0; i < count && i < 20; i++) {
        cJSON* obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "ssid", (char*)records[i].ssid);
        cJSON_AddNumberToObject(obj, "rssi", records[i].rssi);
        cJSON_AddNumberToObject(obj, "auth", records[i].authmode);
        cJSON_AddItemToArray(array, obj);
    }
    
    free(records);
    
    char* json = cJSON_PrintUnformatted(array);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    
    free(json);
    cJSON_Delete(array);
    
    return ESP_OK;
}

esp_err_t WebSettingsServer::HandleConnectWifi(httpd_req_t* req) {
    char buf[512];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';
    
    cJSON* root = cJSON_Parse(buf);
    if (!root) return ESP_FAIL;
    
    cJSON* ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON* pass = cJSON_GetObjectItem(root, "pass");
    
    if (cJSON_IsString(ssid) && cJSON_IsString(pass)) {
        auto& settings = SettingsManager::GetInstance();
        settings.AddWifiCredential(ssid->valuestring, pass->valuestring);
        
        // Try to connect
        wifi_config_t wifi_config = {};
        strncpy((char*)wifi_config.sta.ssid, ssid->valuestring, sizeof(wifi_config.sta.ssid));
        strncpy((char*)wifi_config.sta.password, pass->valuestring, sizeof(wifi_config.sta.password));
        
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        esp_wifi_connect();
        
        httpd_resp_sendstr(req, "{\"status\":\"connecting\"}");
    }
    
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t WebSettingsServer::HandleResetWifi(httpd_req_t* req) {
    auto& settings = SettingsManager::GetInstance();
    settings.ResetToDefaults();
    
    // Clear WiFi config
    wifi_config_t empty_config = {};
    esp_wifi_set_config(WIFI_IF_STA, &empty_config);
    
    httpd_resp_sendstr(req, "{\"status\":\"reset\"}");
    
    // Restart in AP mode after delay
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    
    return ESP_OK;
}

esp_err_t WebSettingsServer::HandleExportSettings(httpd_req_t* req) {
    auto& settings = SettingsManager::GetInstance();
    std::string json = settings.ExportToJson();
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=xiaozhi-settings.json");
    return httpd_resp_send(req, json.c_str(), json.length());
}

esp_err_t WebSettingsServer::HandleImportSettings(httpd_req_t* req) {
    char buf[2048];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';
    
    auto& settings = SettingsManager::GetInstance();
    if (settings.ImportFromJson(buf)) {
        httpd_resp_sendstr(req, "{\"status\":\"imported\"}");
    } else {
        httpd_resp_sendstr(req, "{\"status\":\"error\"}");
    }
    
    return ESP_OK;
}

esp_err_t WebSettingsServer::HandleReboot(httpd_req_t* req) {
    httpd_resp_sendstr(req, "{\"status\":\"rebooting\"}");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}
