// ============================================================
// Web / OTA update module (WebServer + browser upload)
// 基于原 backup/HAL.cpp.bak 重构
// ============================================================
#include "hal.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include "html/ota.h"

static WiFiManager wm;
static WebServer server(80);
static const char* host = "esp32";
static bool ota_active   = false;
static bool wm_running   = false;   // WiFiManager 配网中

// --- WiFi / OTA 初始化 ---
void HAL::WiFi_Init()
{
    WiFi.mode(WIFI_STA);
    wm.setConfigPortalTimeout(180);
}

void HAL::WiFi_Loop()
{
    // 检测到 WIFI_CONNECT 状态时触发连接 (WiFi 任务自行处理, 不阻塞其他任务)
    if (nowApp == AppState::WIFI_CONNECT && !wm_running && !ota_active) {
        HAL::WiFi_Connect();
    }

    // WiFiManager 配网处理
    if (wm_running) {
        wm.process();
        if (WiFi.status() == WL_CONNECTED) {
            wm_running = false;
            HAL::LOG_INFO("WiFi OK: " + WiFi.localIP().toString());
            nowApp = AppState::OTA_UPDATE;
            HAL::OTA_Start();
        }
    }

    // OTA WebServer
    if (ota_active) {
        server.handleClient();
    }
}

// --- WiFi 连接 (WiFiManager 自动处理保存凭据 + AP 回退) ---
void HAL::WiFi_Connect()
{
    if (wm_running || ota_active) return;

    HAL::LOG_INFO("WiFi connecting...");

    // autoConnect: 自动尝试 SPIFFS 中已保存凭据, 失败则启动 AP 配网
    wm.setConfigPortalTimeout(180);
    wm_running = true;

    if (!wm.autoConnect("PD_Power_AP")) {
        // 超时仍未连接
        wm_running = false;
        HAL::LOG_INFO("WiFi FAIL");
        nowApp = AppState::WIFI_FAIL;
    }
    // 成功: wm_running 保持 true, WiFi_Loop 检测后跳转
}

// --- 启动 OTA WebServer ---
void HAL::OTA_Start()
{
    if (ota_active) return;
    if (WiFi.status() != WL_CONNECTED) return;

    MDNS.begin(host);
    HAL::LOG_INFO("OTA: http://" + String(host) + ".local");

    server.on("/info", HTTP_GET, []() {
        String json = "{";
        json += "\"version\":\"" + String(SOFTWARE_VERSION) + "\",";
        json += "\"freeFlash\":" + String(ESP.getFreeSketchSpace() / 1024) + ",";
        json += "\"SNID\":\"" + String((uint32_t)(SNID >> 32), HEX) + String((uint32_t)SNID, HEX) + "\",";
        json += "\"ipAddress\":\"" + WiFi.localIP().toString() + "\"";
        json += "}";
        server.send(200, "application/json", json);
    });

    server.on("/", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", serverIndex);
    });

    server.on("/update", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP.restart();
    }, []() {
        HTTPUpload& upload = server.upload();
        static uint32_t totalSize = 0, written = 0;

        if (upload.status == UPLOAD_FILE_START) {
            totalSize = server.arg("fileSize").toInt();
            written = 0;
            OTA_Progress = 0;
            HAL::LOG_INFO("OTA upload: " + String(totalSize) + " bytes");
            Update.begin(UPDATE_SIZE_UNKNOWN);
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) == upload.currentSize) {
                written += upload.currentSize;
                if (totalSize > 0) OTA_Progress = written * 100 / totalSize;
            }
            vTaskDelay(1);  // 让出 CPU 给 IDLE0，防止 WDT
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) {
                OTA_Progress = 100;
                nowApp = AppState::OTA_FINISH;
                HAL::LOG_INFO("OTA done");
            } else {
                nowApp = AppState::OTA_FAIL;
                HAL::LOG_INFO("OTA failed");
            }
        }
    });

    // 兜底: 未知请求返回 404 (避免浏览器 favicon 等报错)
    server.onNotFound([]() {
        server.send(404, "text/plain", "Not Found");
    });

    server.begin();
    ota_active = true;
    HAL::LOG_INFO("OTA server started");
}

void HAL::OTA_Check()
{
    // 预留
}
