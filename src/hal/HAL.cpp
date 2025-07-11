#include "HAL.h"
#include "Library.h"
#include "Config.h"
#include "html/ota.h"

HAL::OTA_status_t OTA_Status;
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

    Now_App = 11;
}

/* System Loop */
void HAL::Sys_Run() {
    HAL::INA22x_Run();
    HAL::GPIO_Run();
    if(OTA_Status.OTARun == 1){server.handleClient();} // 异步进入OTA
    digitalWrite(LCD_BL_PIN,50);
    switch (Now_App)
    {
    case AppState::Main:
        HAL::UI_Main();
        break;
    case AppState::VBUS_Curve:
        HAL::UI_VBUS_Curve();
        break;
    case AppState::Menu:
        HAL::UI_Menu();
        break;
    case AppState::Log:
        HAL::UI_LOG();
        break;
    case AppState::PowerDelivery:
        HAL::UI_PowerDelivery();
        break;
    case AppState::QuickCharge:
        HAL::UI_QuickCharge();
        break;
    case AppState::SystemInfo:
        HAL::UI_SystemInfo();
        break;
    case AppState::Setting:
        HAL::UI_Setting();
        break;
    case AppState::WiFi_Connect:
        HAL::UI_WiFi_Connect();
        break;
    case AppState::WiFi_Connect_Fail:
        HAL::UI_WiFi_Connect_Fail();
        break;  
    case AppState::OTA_Update:
        while(OTA_Status.OTARun ==0)
        {
            HAL::WebUpdate();
            HAL::UI_OTA_Update();
            OTA_Status.OTARun =1;
        }
        break;   
    case AppState::OTA_Finish:
        HAL::UI_OTA_Finish();
        break;
    case AppState::OTA_Fail:
        HAL::UI_OTA_Fail();
        break;
    default:
        HAL::UI_Main();
        break;
    }
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
    HAL::UI_WiFi_Connect();
    //显示
    Serial.println("WiFi Mode:" + String(WiFi.getMode()));
    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - connect_time_out >= 15*1000)
        {
            HAL::LCD_Refresh_Screen();
            HAL::UI_WiFi_Connect_Fail();
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
        json += "\"SNID\":\"" + String(SNID, HEX) + "\",";
        json += "\"ipAddress\":\"" + ipAddress + "\",";
        json += "\"firmwareMD5\":\"" + SketchMD5 + "\"";
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
        server.send(200, "text/html", serverIndex); // serverIndex 前端HTML
    });

    // 上传固件
    server.on("/update", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        delay(200); // 给前端一点时间显示完成状态
        ESP.restart();
    }, []() {
        HTTPUpload& upload = server.upload();

        static uint32_t totalUpdateSize = 0; // 静态变量记录已上传的大小
        static int totalFileSize = 0; // 静态变量记录固件总大小
        static uint32_t UpdateBlockSize = 1436; // 分块大小

        if (upload.status == UPLOAD_FILE_START) {

            String fileSizeParam = server.arg("fileSize");
            if (fileSizeParam != "") {
                totalFileSize = fileSizeParam.toInt();
            } else {
                // 备用方案
                totalFileSize = server.header("Content-Length").toInt();
            }
            
            Serial.printf("文件名称: %s\n", upload.filename.c_str());
            Serial.printf("固件大小(Byte):%u\n", totalFileSize);
            Serial.print("开始写入...");
            Serial.println("Received headers:");

            OTA_Progress = 0;
            HAL::UI_OTA_Update(); // 强制重新更新一下

            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                Update.printError(Serial);
            }

        }
        else if (upload.status == UPLOAD_FILE_WRITE) {
            // 写入固件数据
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }

            // 计算进度百分比
            if (upload.totalSize > 0) {
                totalUpdateSize += upload.currentSize;
                OTA_Progress = (totalUpdateSize * 100) / totalFileSize;
                // OTA_Progress = 100 - ((upload.currentSize *100) / upload.totalSize); // 计算当前上传进度
            }

            HAL::UI_OTA_Update();
        }
        else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) {
                OTA_Progress = 100;
                HAL::UI_OTA_Finish();
                Serial.printf("更新完成大小(Byte): %u\n", upload.totalSize);
                Serial.print("服务关闭,即将重启");
                delay(5000); // 5s重启
            } else {
                HAL::UI_OTA_Fail();
                Update.printError(Serial);
            }
        }
    });

    server.begin();
    // OTA_Update_Status = 1; // 标记OTA更新状态为进行中
}