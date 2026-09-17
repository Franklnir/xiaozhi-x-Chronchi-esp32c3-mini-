#include "youtube_audio_poller.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_heap_caps.h>
#include <cJSON.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>

#include "application.h"
#include "board.h"
#include "display.h"
#include "system_info.h"
#include "audio/audio_service.h"
#include "protocols/protocol.h"
#include "assets/lang_config.h"

static const char* TAG = "YTPoller";
static const char* kBaseUrl = "https://xiaozhiscig.biz.id";
// HTTP via Nginx (port 80) for streaming - avoids TLS overhead (~20KB saved)
// Nginx handles chunked encoding properly from uvicorn
static const char* kStreamUrl = "http://xiaozhiscig.biz.id";

YouTubeAudioPoller::YouTubeAudioPoller() {
}

YouTubeAudioPoller::~YouTubeAudioPoller() {
    Stop();
}

YouTubeAudioPoller& YouTubeAudioPoller::GetInstance() {
    static YouTubeAudioPoller instance;
    return instance;
}

void YouTubeAudioPoller::Start() {
    if (running_.load()) {
        return;
    }
    running_ = true;
    abort_requested_ = false;

    xTaskCreate([](void* arg) {
        static_cast<YouTubeAudioPoller*>(arg)->TaskLoop();
        vTaskDelete(NULL);
    }, "yt_poller", 5632, this, 2, &task_handle_);
    ESP_LOGI(TAG, "YouTubeAudioPoller started");
}

void YouTubeAudioPoller::Stop() {
    if (!running_.load()) {
        return;
    }
    running_ = false;
    abort_requested_ = true;
    if (task_handle_ != nullptr) {
        task_handle_ = nullptr;
    }
    ESP_LOGI(TAG, "YouTubeAudioPoller stopped");
}

void YouTubeAudioPoller::Abort() {
    if (is_playing_.load()) {
        ESP_LOGI(TAG, "Abort requested for YouTube audio stream");
        abort_requested_ = true;
        Application::GetInstance().GetAudioService().ResetDecoder();
    }
}

void YouTubeAudioPoller::TriggerFetch() {
    ESP_LOGI(TAG, "Fetch triggered externally");
    trigger_requested_ = true;
}

void YouTubeAudioPoller::PlaySong(const std::string& video_id, const std::string& title) {
    if (video_id.empty()) {
        return;
    }
    ESP_LOGI(TAG, "PlaySong called directly: title=\"%s\", vid=%s", title.c_str(), video_id.c_str());
    {
        std::lock_guard<std::mutex> lock(song_mutex_);
        pending_video_id_ = video_id;
        pending_title_ = title.empty() ? "YouTube Song" : title;
    }
    play_requested_ = true;
}

void YouTubeAudioPoller::PlayQuery(const std::string& query) {
    if (query.empty()) {
        return;
    }
    // Guard: ignore if already playing or a play is pending (prevents TTS text triggering duplicate)
    if (is_playing_.load() || play_requested_.load()) {
        ESP_LOGW(TAG, "PlayQuery ignored (already playing or pending): \"%s\"", query.c_str());
        return;
    }
    ESP_LOGI(TAG, "PlayQuery called: query=\"%s\"", query.c_str());
    {
        std::lock_guard<std::mutex> lock(song_mutex_);
        pending_query_ = query;
    }
    query_requested_ = true;
}

bool YouTubeAudioPoller::FetchDirect(const std::string& query, std::string& video_id, std::string& title) {
    auto network = Board::GetInstance().GetNetwork();
    if (network == nullptr) return false;

    auto http = network->CreateHttp(0);
    if (http == nullptr) return false;

    std::string encoded_q;
    for (char c : query) {
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded_q += c;
        } else if (c == ' ') {
            encoded_q += "%20";
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
            encoded_q += hex;
        }
    }

    std::string url = std::string(kBaseUrl) + "/api/audio/play_direct?q=" + encoded_q;
    ESP_LOGI(TAG, "FetchDirect querying URL: %s", url.c_str());
    http->SetTimeout(12000);
    http->SetHeader("User-Agent", "ESP32-XiaoZhi");

    if (!http->Open("GET", url)) {
        ESP_LOGW(TAG, "FetchDirect failed to open: %s", url.c_str());
        http->Close();
        return false;
    }

    if (http->GetStatusCode() != 200) {
        ESP_LOGW(TAG, "FetchDirect HTTP returned %d", http->GetStatusCode());
        http->Close();
        return false;
    }

    std::string body = http->ReadAll();
    http->Close();

    if (body.empty()) return false;

    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) return false;

    bool found = false;
    cJSON* vid_item = cJSON_GetObjectItem(root, "video_id");
    cJSON* title_item = cJSON_GetObjectItem(root, "title");
    if (cJSON_IsString(vid_item)) {
        video_id = vid_item->valuestring;
        title = cJSON_IsString(title_item) ? title_item->valuestring : query;
        found = true;
    }
    cJSON_Delete(root);
    return found;
}

bool YouTubeAudioPoller::FetchCommands(int& cmd_id, std::string& video_id, std::string& title) {
    auto network = Board::GetInstance().GetNetwork();
    if (network == nullptr) {
        return false;
    }

    auto http = network->CreateHttp(0);
    if (http == nullptr) {
        return false;
    }

    std::string mac = SystemInfo::GetMacAddress();
    std::string url = std::string(kBaseUrl) + "/api/device/audio/commands?mac=" + mac;

    http->SetTimeout(10000);
    http->SetHeader("User-Agent", "ESP32-XiaoZhi");

    if (!http->Open("GET", url)) {
        ESP_LOGW(TAG, "FetchCommands: Failed to connect to %s", url.c_str());
        http->Close();
        return false;
    }

    if (http->GetStatusCode() != 200) {
        ESP_LOGW(TAG, "FetchCommands: HTTP returned status %d", http->GetStatusCode());
        http->Close();
        return false;
    }

    std::string body = http->ReadAll();
    http->Close();

    if (body.empty()) {
        return false;
    }

    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        return false;
    }

    bool found = false;
    cJSON* commands = cJSON_GetObjectItem(root, "commands");
    if (cJSON_IsArray(commands) && cJSON_GetArraySize(commands) > 0) {
        cJSON* cmd = cJSON_GetArrayItem(commands, 0);
        if (cmd != nullptr) {
            cJSON* id_item = cJSON_GetObjectItem(cmd, "id");
            cJSON* vid_item = cJSON_GetObjectItem(cmd, "video_id");
            cJSON* title_item = cJSON_GetObjectItem(cmd, "title");

            if (cJSON_IsNumber(id_item) && cJSON_IsString(vid_item)) {
                cmd_id = id_item->valueint;
                video_id = vid_item->valuestring;
                title = cJSON_IsString(title_item) ? title_item->valuestring : "YouTube Song";
                found = true;
            }
        }
    }

    cJSON_Delete(root);
    return found;
}

bool YouTubeAudioPoller::AckCommand(int cmd_id) {
    auto network = Board::GetInstance().GetNetwork();
    if (network == nullptr) {
        return false;
    }

    auto http = network->CreateHttp(0);
    if (http == nullptr) {
        return false;
    }

    std::string mac = SystemInfo::GetMacAddress();
    std::string url = std::string(kBaseUrl) + "/api/device/audio/ack";

    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "command_id", cmd_id);
    cJSON_AddStringToObject(root, "mac", mac.c_str());
    char* json_str = cJSON_PrintUnformatted(root);
    std::string payload = json_str ? json_str : "";
    if (json_str) {
        free(json_str);
    }
    cJSON_Delete(root);

    http->SetTimeout(10000);
    http->SetHeader("Content-Type", "application/json");
    http->SetHeader("User-Agent", "ESP32-XiaoZhi");
    http->SetContent(std::move(payload));

    bool ok = http->Open("POST", url);
    if (ok) {
        http->ReadAll();
    }
    http->Close();
    return ok;
}

void YouTubeAudioPoller::StreamAudio(const std::string& video_id, const std::string& title) {
    auto& app = Application::GetInstance();
    auto display = Board::GetInstance().GetDisplay();

    is_playing_ = true;
    abort_requested_ = false;
    app.ResetAborted();

    ESP_LOGI(TAG, "Starting YouTube stream: %s (%s)", title.c_str(), video_id.c_str());

    // Wait for TTS speaking to finish (max 8 sec) before opening stream
    // This frees MQTT audio memory and prevents audio output conflict
    {
        int wait_ms = 0;
        while (app.GetDeviceState() == kDeviceStateSpeaking && wait_ms < 8000 && !abort_requested_.load()) {
            vTaskDelay(pdMS_TO_TICKS(100));
            wait_ms += 100;
        }
        if (wait_ms > 0) {
            ESP_LOGI(TAG, "Waited %d ms for TTS to finish before streaming", wait_ms);
        }
    }

    if (abort_requested_.load()) {
        is_playing_ = false;
        return;
    }

    // Close any open audio channel to the server so mic does not record speaker output or hit 30s listening timeout
    app.CloseAudioChannel();
    app.SetDeviceState(kDeviceStateIdle);

    // Clear any pending play/query requests that accumulated during TTS wait
    play_requested_ = false;
    query_requested_ = false;

    std::string display_title = "🎵 " + title;
    if (display != nullptr) {
        display->SetChatMessage("assistant", display_title.c_str());
        display->ShowNotification(display_title.c_str(), 10000);
    }

    auto codec = Board::GetInstance().GetAudioCodec();
    if (codec != nullptr) {
        if (!codec->output_enabled()) {
            codec->EnableOutput(true);
        }
        if (codec->output_volume() < 60) {
            codec->SetOutputVolume(70);
        }
    }

    Board::GetInstance().SetPowerSaveLevel(PowerSaveLevel::PERFORMANCE);

    // Reset decoder to clear any leftover TTS audio in the queue
    app.GetAudioService().ResetDecoder();
    vTaskDelay(pdMS_TO_TICKS(50));

    auto network = Board::GetInstance().GetNetwork();
    if (network == nullptr) {
        is_playing_ = false;
        Board::GetInstance().SetPowerSaveLevel(PowerSaveLevel::LOW_POWER);
        return;
    }

    // 1. Initial Bitrate Evaluation based on Wi-Fi RSSI & Free Internal SRAM
    wifi_ap_record_t ap_info;
    int8_t rssi = -100;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        rssi = ap_info.rssi;
    }
    size_t free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    std::string current_bitrate = "12k";
    if (rssi >= -65 && free_sram >= 48000) {
        current_bitrate = "16k";
    } else if (rssi >= -75 && free_sram >= 36000) {
        current_bitrate = "12k";
    } else if (rssi >= -82 && free_sram >= 28000) {
        current_bitrate = "8k";
    } else {
        current_bitrate = "6k";
    }
    ESP_LOGI(TAG, "Adaptive Profile Selected: RSSI=%d dBm, FreeSRAM=%u B -> Bitrate=%s",
             rssi, (unsigned int)free_sram, current_bitrate.c_str());

    static const uint8_t OGGS_MAGIC[4] = {'O', 'g', 'g', 'S'};
    static const size_t kOggBufCap = 4096;
    auto ogg_buf = std::make_unique<uint8_t[]>(kOggBufCap);

    size_t total_bytes_streamed = 0;
    int frames_pushed = 0;
    int downshift_retries = 0;

    while (running_.load() && !abort_requested_.load()) {
        auto http = network->CreateHttp(0);
        if (http == nullptr) {
            break;
        }

        float start_sec = frames_pushed * 0.060f;
        char url_buf[256];
        if (start_sec > 0.5f) {
            snprintf(url_buf, sizeof(url_buf), "%s/api/audio/stream/%s?br=%s&start=%.2f",
                     kStreamUrl, video_id.c_str(), current_bitrate.c_str(), start_sec);
        } else {
            snprintf(url_buf, sizeof(url_buf), "%s/api/audio/stream/%s?br=%s",
                     kStreamUrl, video_id.c_str(), current_bitrate.c_str());
        }
        std::string stream_url(url_buf);

        http->SetTimeout(15000);
        http->SetHeader("User-Agent", "ESP32-XiaoZhi");
        http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());

        ESP_LOGI(TAG, "Connecting to stream URL: %s", stream_url.c_str());
        if (!http->Open("GET", stream_url)) {
            ESP_LOGE(TAG, "Failed to open stream URL: %s", stream_url.c_str());
            http->Close();
            if (frames_pushed > 0 && downshift_retries < 2) {
                downshift_retries++;
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            break;
        }

        if (http->GetStatusCode() != 200) {
            ESP_LOGE(TAG, "Stream returned HTTP %d", http->GetStatusCode());
            http->Close();
            if (frames_pushed > 0 && downshift_retries < 2) {
                downshift_retries++;
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            break;
        }

        downshift_retries = 0;
        ESP_LOGI(TAG, "Stream connected (HTTP 200, br=%s, start=%.2fs), reading chunks...",
                 current_bitrate.c_str(), start_sec);

        size_t ogg_len = 0;
        bool seen_head = false;
        bool seen_tags = false;
        int sample_rate = 24000;
        bool need_downshift = false;
        int starvation_count = 0;

        try {
            while (running_.load() && !abort_requested_.load()) {
                // Adaptive Buffer Health Check
                size_t q_size = app.GetAudioService().GetDecodeQueueSize();
                if (frames_pushed > 20 && q_size == 0) {
                    starvation_count++;
                    if (starvation_count >= 5) {
                        if (current_bitrate == "16k") {
                            current_bitrate = "12k";
                            need_downshift = true;
                        } else if (current_bitrate == "12k") {
                            current_bitrate = "8k";
                            need_downshift = true;
                        } else if (current_bitrate == "8k") {
                            current_bitrate = "6k";
                            need_downshift = true;
                        }
                        if (need_downshift) {
                            ESP_LOGW(TAG, "Buffer starvation detected! Downshifting to %s at %.2fs to prevent stuttering",
                                     current_bitrate.c_str(), frames_pushed * 0.060f);
                            break;
                        }
                    }
                } else if (q_size >= 3) {
                    starvation_count = 0;
                }

                size_t avail = kOggBufCap - ogg_len;
                if (avail == 0) {
                    // Buffer is full without a parsed page. Discard until next OggS magic.
                    size_t next_oggs = 1;
                    while (next_oggs + 4 <= ogg_len && memcmp(ogg_buf.get() + next_oggs, OGGS_MAGIC, 4) != 0) {
                        next_oggs++;
                    }
                    if (next_oggs < ogg_len) {
                        memmove(ogg_buf.get(), ogg_buf.get() + next_oggs, ogg_len - next_oggs);
                        ogg_len -= next_oggs;
                    } else {
                        ogg_len = 0;
                    }
                    avail = kOggBufCap - ogg_len;
                }
                int to_read = (int)std::min((size_t)512, avail);
                int n = http->Read(reinterpret_cast<char*>(ogg_buf.get() + ogg_len), to_read);
                if (n < 0) {
                    ESP_LOGW(TAG, "HTTP read error or stream closed (ret=%d, total=%zu, frames=%d)",
                             n, total_bytes_streamed, frames_pushed);
                    if (frames_pushed > 10) {
                        if (current_bitrate != "6k") {
                            if (current_bitrate == "16k") current_bitrate = "12k";
                            else if (current_bitrate == "12k") current_bitrate = "8k";
                            else current_bitrate = "6k";
                            need_downshift = true;
                            ESP_LOGW(TAG, "Connection interrupted mid-stream! Downshifting to %s at %.2fs",
                                     current_bitrate.c_str(), frames_pushed * 0.060f);
                        } else if (downshift_retries < 2) {
                            need_downshift = true;
                            downshift_retries++;
                            ESP_LOGW(TAG, "Connection interrupted at 6k! Reconnecting at %.2fs",
                                     frames_pushed * 0.060f);
                        }
                    }
                    break;
                }
                if (n == 0) {
                    ESP_LOGI(TAG, "Stream EOF reached (total=%zu, frames=%d)", total_bytes_streamed, frames_pushed);
                    break;
                }

                ogg_len += n;
                total_bytes_streamed += n;

                // Demux Ogg pages incrementally
                while (ogg_len >= 27) {
                    if (abort_requested_.load()) {
                        ESP_LOGI(TAG, "Audio stream aborted by user request");
                        break;
                    }

                    // Find next OggS magic
                    size_t oggs_pos = 0;
                    bool found = false;
                    while (oggs_pos + 4 <= ogg_len) {
                        if (memcmp(ogg_buf.get() + oggs_pos, OGGS_MAGIC, 4) == 0) {
                            found = true;
                            break;
                        }
                        oggs_pos++;
                    }

                    if (!found) {
                        if (ogg_len >= 3) {
                            memmove(ogg_buf.get(), ogg_buf.get() + ogg_len - 3, 3);
                            ogg_len = 3;
                        }
                        break;
                    }

                    if (oggs_pos > 0) {
                        memmove(ogg_buf.get(), ogg_buf.get() + oggs_pos, ogg_len - oggs_pos);
                        ogg_len -= oggs_pos;
                        if (ogg_len < 27) break;
                    }

                    uint8_t num_segs = ogg_buf[26];
                    size_t hdr_len = 27 + num_segs;
                    if (ogg_len < hdr_len) {
                        break;
                    }

                    size_t body_len = 0;
                    for (size_t i = 0; i < num_segs; ++i) {
                        body_len += ogg_buf[27 + i];
                    }

                    size_t page_total = hdr_len + body_len;
                    if (ogg_len < page_total) {
                        break;
                    }

                    // Extract packets from this page
                    size_t cur = hdr_len;
                    size_t seg_idx = 0;
                    while (seg_idx < num_segs) {
                        size_t pkt_len = 0;
                        size_t pkt_start = cur;
                        bool continued = false;
                        do {
                            uint8_t l = ogg_buf[27 + seg_idx++];
                            pkt_len += l;
                            cur += l;
                            continued = (l == 255);
                        } while (continued && seg_idx < num_segs);

                        if (pkt_len == 0) {
                            continue;
                        }

                        const uint8_t* pkt_ptr = ogg_buf.get() + pkt_start;

                        if (!seen_head) {
                            if (pkt_len >= 19 && memcmp(pkt_ptr, "OpusHead", 8) == 0) {
                                seen_head = true;
                                if (pkt_len >= 16) {
                                    sample_rate = pkt_ptr[12] | (pkt_ptr[13] << 8) | (pkt_ptr[14] << 16) | (pkt_ptr[15] << 24);
                                    ESP_LOGI(TAG, "OpusHead: sample_rate=%d", sample_rate);
                                }
                            }
                            continue;
                        }

                        if (!seen_tags) {
                            if (pkt_len >= 8 && memcmp(pkt_ptr, "OpusTags", 8) == 0) {
                                seen_tags = true;
                                ESP_LOGI(TAG, "OpusTags parsed, ready to decode audio");
                            }
                            continue;
                        }

                        if (abort_requested_.load()) {
                            ESP_LOGI(TAG, "Audio stream aborted by user request");
                            break;
                        }

                        // Push raw Opus frame to decoder queue (blocking with true for flow control)
                        auto packet = std::make_unique<AudioStreamPacket>();
                        packet->sample_rate = 24000;
                        packet->frame_duration = 60;
                        packet->payload.assign(pkt_ptr, pkt_ptr + pkt_len);
                        app.GetAudioService().PushPacketToDecodeQueue(std::move(packet), true);
                        frames_pushed++;
                        if (frames_pushed == 1 || frames_pushed % 50 == 0) {
                            ESP_LOGI(TAG, "Audio frame #%d pushed to decoder (len=%zu, q_size=%zu)",
                                     frames_pushed, pkt_len, app.GetAudioService().GetDecodeQueueSize());
                        }
                    }

                    memmove(ogg_buf.get(), ogg_buf.get() + page_total, ogg_len - page_total);
                    ogg_len -= page_total;

                    if (abort_requested_.load()) {
                        ESP_LOGI(TAG, "Audio stream aborted by user request");
                        break;
                    }
                }
            }
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "StreamAudio exception: %s", e.what());
        }

        http->Close();

        if (!need_downshift || abort_requested_.load()) {
            break;
        }
    }
    ESP_LOGI(TAG, "Stream finished (total %zu bytes read, frames_pushed=%d, aborted=%d)",
             total_bytes_streamed, frames_pushed, abort_requested_.load());

    if (!abort_requested_.load()) {
        app.GetAudioService().WaitForPlaybackIdle(5000);
    } else {
        app.GetAudioService().ResetDecoder();
    }

    // Cleanly restore device state and UI
    app.SetDeviceState(kDeviceStateIdle);
    app.GetAudioService().EnableWakeWordDetection(true);

    if (display != nullptr) {
        display->SetChatMessage("assistant", "");
        display->SetChatMessage("system", "");
        display->SetStatus(Lang::Strings::STANDBY);
        display->SetEmotion("neutral");
        display->SetFaceState(FaceState::Idle);
    }

    Board::GetInstance().SetPowerSaveLevel(PowerSaveLevel::LOW_POWER);

    is_playing_ = false;
}

void YouTubeAudioPoller::TaskLoop() {
    ESP_LOGI(TAG, "YouTube player ready (on-demand direct playback)");

    while (running_.load()) {
        // Wait for direct tool call (play_requested_), direct query (query_requested_), or trigger (trigger_requested_)
        while (running_.load() && !play_requested_.load() && !query_requested_.load() && !trigger_requested_.load()) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if (!running_.load()) {
            break;
        }

        // Direct MCP tool call has highest priority
        if (play_requested_.exchange(false)) {
            std::string vid, tit;
            {
                std::lock_guard<std::mutex> lock(song_mutex_);
                vid = pending_video_id_;
                tit = pending_title_;
            }
            if (!vid.empty()) {
                ESP_LOGI(TAG, "Direct tool call playing: \"%s\" (vid=%s)", tit.c_str(), vid.c_str());
                StreamAudio(vid, tit);
            }
        }
        // Direct voice query search
        else if (query_requested_.exchange(false)) {
            std::string q;
            {
                std::lock_guard<std::mutex> lock(song_mutex_);
                q = pending_query_;
            }
            if (!q.empty()) {
                ESP_LOGI(TAG, "Searching & streaming song query: \"%s\"", q.c_str());
                std::string vid, tit;
                if (FetchDirect(q, vid, tit)) {
                    StreamAudio(vid, tit);
                }
            }
        }
        // Fallback queue trigger
        else if (trigger_requested_.exchange(false)) {
            int cmd_id = 0;
            std::string video_id;
            std::string title;

            if (FetchCommands(cmd_id, video_id, title)) {
                ESP_LOGI(TAG, "Playing requested YouTube song (1 song only): id=%d, title=\"%s\", vid=%s",
                         cmd_id, title.c_str(), video_id.c_str());
                AckCommand(cmd_id);
                StreamAudio(video_id, title);
            }
        }
    }
}
