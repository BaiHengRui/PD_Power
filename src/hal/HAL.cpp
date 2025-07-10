#include "HAL.h"
#include "Library.h"
#include "GlobalVariables.h"
#include "Config.h"
#include "html/ota.h"

const char* host = "esp32";
WebServer server(80);
WiFiManager wm;

long connect_time_out;

/* System Init */
void HAL::Sys_Init() {
    SNID = ESP.getEfuseMac();
    Free_Flash_Size = ESP.getFreeSketchSpace() / 1024; // Remaining flash size in KB
    Sketch_Size = ESP.getSketchSize() / 1024; // Program size in KB
    GPIO_Init();
    INA22x_Init();
    PD_Init();
    Buzzer_Init();
    LCD_Init();
}

/* System Loop */
void HAL::Sys_Run() {
    INA22x_Run();
    GPIO_Run();
    if(OTA_Update_Status == 1){server.handleClient();}
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

/* OTA */
void HAL::WebUpdate(){
    connect_time_out = millis();
    // Serial.println("Web OTA Mode");
    ReadWiFiConfig();
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
        SaveWiFiConfig();
        ReadWiFiConfig();
        Serial.print("IP地址: ");
        Serial.println(WiFi.localIP());
    }
    
    if (!MDNS.begin(host))//host = 80(http)
    {
        Serial.println("mDNS服务启动失败！");
        while (1)
        {
            delay(1000);
        }
    }
    Serial.println("mDNS服务启动成功！");
    Serial.print("局域网OTA更新网址: ");
    Serial.println("http://esp32.local");
    //网页服务
    server.on("/",HTTP_GET,[](){
        server.sendHeader("Connection","close");
        server.send(200,"text/html",serverIndex);
    });
    //上传固件
    server.on("/update", HTTP_POST,[](){
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
    },[](){
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START)
        {
            Serial.printf("文件名称: %s\n", upload.filename.c_str());
            Serial.printf("当前写入大小(Byte):%u\n",upload.currentSize);
            Serial.print("开始写入...");
            if (!Update.begin(UPDATE_SIZE_UNKNOWN))
            {
                Update.printError(Serial);
            }  
        }else if (upload.status == UPLOAD_FILE_WRITE)
        {
            //写入固件
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            {
                Update.printError(Serial);
            }
            static int LastProgress = 0;//上一次进度
            if (upload.totalSize >0)// 避免除零错误
            {
                OTA_Progress = 100 - ((upload.currentSize *100) / upload.totalSize);//计算当前上传进度
                //显示状态
                if (OTA_Progress != LastProgress) //更新进度
                {
                    LastProgress = OTA_Progress;
                }
            } 
        }else if (upload.status == UPLOAD_FILE_END)
        {
            if (Update.end(true))
            {
                //显示结束
                OTA_Progress = 0;
                OTA_Update_Status = 0;
                Serial.printf("更新完成大小(Byte): %u\n", upload.totalSize);
                Serial.print("服务关闭,即将重启");
                delay(5000);//5s重启
            }else
            {
                //显示OTA失败
                Update.printError(Serial);
            }
        }
    });
    server.begin();
    
}