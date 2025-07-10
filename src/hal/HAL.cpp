#include "HAL.h"
#include "Library.h"
#include "Config.h"
#include "html/ota.h"

const char* host = "esp32";
WebServer server(80);
WiFiManager wm;
struct config_type
{
    char ssid[32]; // 配网WiFi的SSID
    char psw[64];  // 配网WiFi的Password
};config_type wificonf = {{""}, {""}};
long connect_time_out;

/* System Init */
void HAL::Sys_Init() {
    Serial.begin(115200);
    EEPROM.begin(1024); // Initialize EEPROM with 1024 bytes
    Wire.begin(I2C_SDA_PIN,I2C_SCL_PIN);
    HAL::GPIO_Init();
    HAL::INA22x_Init();
    HAL::PD_Init();
    HAL::Buzzer_Init();
    HAL::LCD_Init();
    SNID = ESP.getEfuseMac();
    Free_Flash_Size = ESP.getFreeSketchSpace() / 1024; // Remaining flash size in KB
    Sketch_Size = ESP.getSketchSize() / 1024; // Program size in KB
    Serial.println("System Init Complete!");

    HAL::WebUpdate();
    OTA_Update_Status =1;
}

/* System Loop */
void HAL::Sys_Run() {
    HAL::INA22x_Run();
    HAL::GPIO_Run();
    if(OTA_Update_Status == 1){server.handleClient();}
    HAL::UI_VBUS_Curve();
    digitalWrite(LCD_BL_PIN,50);
}

/* Save WiFi to EEPROM */
void HAL::SaveWiFiConfig(){
    uint8_t *p = (uint8_t*)(&wificonf);
    for (int i = 0; i < sizeof(wificonf); i++)
    {
      EEPROM.write(i + EEPROM_WiFi_addr, *(p + i));
    }
    delay(10);
    EEPROM.commit();
    delay(10);
}

/* Read WiFi to EEPROM */
void HAL::ReadWiFiConfig(){
    uint8_t *p = (uint8_t*)(&wificonf);
    for (int i = 0; i < sizeof(wificonf); i++)
    {
      *(p + i) = EEPROM.read(i + EEPROM_WiFi_addr);
    }
}

/* Delete WiFi to EEPROM */
void HAL::DeleteWiFiConfig(){
    config_type deletewifi = {{""}, {""}};
    uint8_t *p = (uint8_t*)(&deletewifi);
    for (int i = 0; i < sizeof(deletewifi); i++)
    {
      EEPROM.write(i + EEPROM_WiFi_addr, *(p + i));
    }
    delay(10);
    EEPROM.commit();
    delay(10);
}

void HAL::WiFiConnect(){
    connect_time_out = millis();
    // Serial.println("Web OTA Mode");
    HAL::ReadWiFiConfig();
    WiFi.begin(wificonf.ssid,wificonf.psw);
    //显示
    Serial.println("WiFi Mode:" + String(WiFi.getMode()));
    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - connect_time_out >= 15*1000)
        {
            std::vector<const char *> menu = {"wifi","restart"};
            wm.setMenu(menu);

            bool res;
            res = wm.autoConnect("ESP32AP","");
            if (!res)
            {
                Serial.println("WiFi连接失败！");
                while(res);
            }else
            {
                Serial.println("WiFi连接成功！");
            }
            
        }
        
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print("WiFi名称:");
        Serial.println(WiFi.SSID().c_str());
        Serial.print("WiFi密码:");
        Serial.println(WiFi.psk().c_str());
        strcpy(wificonf.ssid,WiFi.SSID().c_str());
        stpcpy(wificonf.psw,WiFi.psk().c_str());
        HAL::SaveWiFiConfig();
        HAL::ReadWiFiConfig();
        Serial.print("IP地址: ");
        Serial.println(WiFi.localIP());
    }
    
}

/* OTA */
void HAL::WebUpdate() {
    HAL::WiFiConnect();
    if (!MDNS.begin(host)) {
        Serial.println("mDNS服务初始化失败!");
        while (1) {
            delay(1000);
        }
    }
    Serial.println("mDNS服务初始化成功!");
    Serial.print("本地升级URL: http://");
    Serial.print(host);
    Serial.println(".local");

    // 添加设备信息端点
    server.on("/info", HTTP_GET, []() {
        String ipAddress = WiFi.localIP().toString();
        String SketchMD5 = ESP.getSketchMD5();
        // 创建JSON响应
        String json = "{";
        json += "\"version\":\"" + String(FirmwareVer) + "\",";
        json += "\"freeFlash\":" + String(ESP.getFreeSketchSpace() / 1024) + ",";
        json += "\"SNID\":\"" + String(SNID,HEX) + "\",";
        json += "\"ipAddress\":\"" + ipAddress + "\"";
        // json += "\"SketchMD5\":\"" + String(SketchMD5) + "\"";
        json += "}";
        
        server.send(200, "application/json", json);
    });

    // 添加进度查询端点
    server.on("/progress", HTTP_GET, []() {
        server.send(200, "text/plain", String(OTA_Progress));
    });

    // 网页服务
    server.on("/", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", serverIndex); // serverIndex 是美化后的前端HTML
    });

    // 上传固件
    server.on("/update", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        delay(1000); // 给前端一点时间显示完成状态
        ESP.restart();
    }, []() {
        HTTPUpload& upload = server.upload();
        static uint32_t totalSize = 0;       // 总大小
        static uint32_t writtenSize = 0;     // 已写入大小

        if (upload.status == UPLOAD_FILE_START) {
            Serial.printf("Update Start: %s\n", upload.filename.c_str());
            totalSize = upload.totalSize;
            writtenSize = 0;
            OTA_Progress = 0;

            if (Update.begin(UPDATE_SIZE_UNKNOWN)) {
                Serial.println("Update begin successfully");
            } else {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            // 写入固件
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
            writtenSize += upload.currentSize;

            // 计算进度（百分比）
            if (totalSize > 0) {
                OTA_Progress = (writtenSize * 100) / totalSize;
            }
            Serial.printf("Progress: %d%%\n", OTA_Progress);
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) { // true表示更新完成后设置重启标志
                OTA_Progress = 100;
                Serial.printf("Update Success: %u bytes\n", writtenSize);
            } else {
                Update.printError(Serial);
            }
        }
    });

    server.begin();
    // OTA_Update_Status = 1; // 标记OTA更新状态为进行中
}