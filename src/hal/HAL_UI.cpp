#include "hal.h"
#include "Config.h"
#include "Fonts/Font1_12.h"
#include "Fonts/Font1_18.h"
#include "Fonts/Font1_20.h"
#include "Fonts/Font1_40.h"
#include "ui/cloud_download.h"
#include "ui/update_error.h"
#include "ui/update_success.h"
#include "ui/wlan_error.h"

TFT_eSPI tft = TFT_eSPI(240,240);
TFT_eSprite spr = TFT_eSprite(&tft);

#define VNUM_POINTS 160  //VBUS 与格子宽度一致
float VoltageData[VNUM_POINTS] = {0};
float CurrentData[VNUM_POINTS] = {0};
float PowerData[VNUM_POINTS] = {0};

void HAL::LCD_Init(){
    analogWrite(LCD_BL_PIN,0);
    tft.init();
    tft.invertDisplay(LCD_InvertDisplay);
    tft.setRotation(LCD_Rotation);

    int Light_temp = EEPROM.read(EEPROM_Light_addr);
    int Rotation_temp = EEPROM.read(EEPROM_Rotation_addr);
    LCD_Light = (Light_temp >=1 && Light_temp <=100) ? Light_temp : LCD_Light;
    LCD_Rotation = (Rotation_temp >=0 && Rotation_temp <= 3) ? Rotation_temp : LCD_Rotation;
    if(Light_temp >=1 && Light_temp <=100){EEPROM.write(EEPROM_Light_addr,LCD_Light);EEPROM.commit();}
    if(Rotation_temp >=0 && Rotation_temp <=3){EEPROM.write(EEPROM_Rotation_addr,LCD_Rotation);EEPROM.commit();}

    tft.fillScreen(0x0000);
    delay(50); //延时50ms，等待屏幕初始化完成
    HAL::LCD_Light_Updat(LCD_Light,0);
}

// 调整背光亮度，范围1-100，saved为true时保存到EEPROM
void HAL::LCD_Light_Updat(int light, bool saved){
    light = constrain(light,1,100);//限制大小
    int light_pwm = 255 - ((100 - light) * 1.5);
    analogWrite(LCD_BL_PIN,light_pwm);
    if(saved == 1){EEPROM.write(EEPROM_Light_addr,light);EEPROM.commit();}
}

// 调整屏幕旋转，范围0-3，saved为true时保存到EEPROM
void HAL::LCD_Rotation_Update(int rotation, bool saved){
    LCD_Rotation = constrain(rotation,0,3);
    tft.setRotation(LCD_Rotation);
    if(saved == 1){EEPROM.write(EEPROM_Rotation_addr,LCD_Rotation);EEPROM.commit();}
}

// 刷新屏幕为指定背景色
void HAL::LCD_Refresh_Screen(uint32_t bgcolor){
    spr.fillScreen(bgcolor);
}

// 主界面显示
void HAL::UI_Main(){
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(CC_DATUM);
    spr.setColorDepth(8);
    spr.setTextColor(TFT_WHITE);
    spr.loadFont(Font1_40);

    // spr.setTextColor(TFT_WHITE);
    spr.setCursor(5,5);  
    spr.setTextColor(TFT_GREEN);
    spr.print(LoadVoltage, LoadVoltage < 10 ? 5 : 4);
    spr.setCursor(140,5);
    spr.print("V");

    spr.setCursor(5,52); 
    spr.setTextColor(TFT_YELLOW);
    spr.print(LoadCurrent, LoadCurrent < 10 ? 5 : 4);
    spr.setCursor(140,52);
    spr.print("A");

    spr.setCursor(5,97); 
    spr.setTextColor(TFT_CYAN);
    spr.print(LoadPower, LoadPower < 10 ? 5 : (LoadPower < 100 ? 4 : 3));
    spr.setCursor(140,97);
    spr.print("W");
    spr.unloadFont();

    spr.loadFont(Font1_12);
    spr.setCursor(0, 150);
    spr.setTextColor(TFT_YELLOW);
    spr.println("  SET:");
    if (PD_Option == 0)
    {
        spr.println("  FIX: " + String(PD_Voltage, 2) + "V " + String(PD_Current, 2) + "A");
    }else if (PD_Option == 1)
    {
        spr.println("  PPS: " + String(PD_Voltage, 2) + "V " + String(PD_Current, 2) + "A");
    }
    spr.print("  SRC:" + String(PD_Src_Cap_Count));
    spr.print("  POS:" + String(PD_Position));

    spr.setCursor(180,10);
    spr.setTextColor(TFT_WHITE);
    spr.print("FPS:" + String(currentFPS,2));
    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

// VBUS 电压电流曲线显示
void HAL::UI_VBUS_Curve() {
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(CC_DATUM);
    spr.setColorDepth(8);
    spr.setTextColor(TFT_WHITE);
    spr.loadFont(Font1_12);

    // 更新数据缓冲区
    for (int i = VNUM_POINTS - 1; i > 0; i--) {
        VoltageData[i] = VoltageData[i - 1];
        CurrentData[i] = CurrentData[i - 1];
    }
    VoltageData[0] = LoadVoltage * 10;
    CurrentData[0] = LoadCurrent * 10;

    // 安全获取极值函数
    auto safeExtremes = [](float* data, int len) -> std::pair<float, float> {
        if (len == 0) return {0, 0};
        float minVal = data[0], maxVal = data[0];
        for (int i = 1; i < len; ++i) {
            if (data[i] < minVal) minVal = data[i];
            if (data[i] > maxVal) maxVal = data[i];
        }
        return {minVal, maxVal};
    };

    // 获取极值并动态调整量程
    std::pair<float, float> vExtremes = safeExtremes(VoltageData, VNUM_POINTS);
    float vMin = vExtremes.first;
    float vMax = vExtremes.second;
    std::pair<float, float> cExtremes = safeExtremes(CurrentData, VNUM_POINTS);
    float cMin = cExtremes.first;
    float cMax = cExtremes.second;

    // 平滑因子
    const float smoothFactor = 0.1;

    // 电压量程调整
    float vRange = vMax - vMin;
    if (vRange < 1.0f) {
        float center = (vMax + vMin) / 2;
        MinVoltage = (1 - smoothFactor) * MinVoltage + smoothFactor * (center - 0.5f);
        MaxVoltage = (1 - smoothFactor) * MaxVoltage + smoothFactor * (center + 0.5f);
    } else {
        MaxVoltage = (1 - smoothFactor) * MaxVoltage + smoothFactor * (vMax + vRange * 0.1f);
        MinVoltage = (1 - smoothFactor) * MinVoltage + smoothFactor * (vMin - vRange * 0.1f);
    }
    MinVoltage = std::max(MinVoltage, 0.0f);

    // 电流量程调整
    float cRange = cMax - cMin;
    if (cRange < 0.1f) {
        float center = (cMax + cMin) / 2;
        MinCurrent = (1 - smoothFactor) * MinCurrent + smoothFactor * (center - 0.05f);
        MaxCurrent = (1 - smoothFactor) * MaxCurrent + smoothFactor * (center + 0.05f);
    } else {
        MaxCurrent = (1 - smoothFactor) * MaxCurrent + smoothFactor * (cMax + cRange * 0.1f);
        MinCurrent = (1 - smoothFactor) * MinCurrent + smoothFactor * (cMin - cRange * 0.1f);
    }
    MinCurrent = std::max(MinCurrent, 0.0f);
    if (MaxCurrent - MinCurrent < 0.1f) MaxCurrent = MinCurrent + 0.1f;

    // 安全映射函数
    auto safeMap = [](float value, float inMin, float inMax, int outMin, int outMax) {
        if (inMin >= inMax) return (outMin + outMax) / 2;
        return (int)((value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin);
    };

    // 绘制区域参数
    const int graphX = 40;
    const int graphY = 40;
    const int graphWidth = 160;
    const int graphHeight = 160;

    // 绘制刻度和网格线
    auto drawScale = [&](float minVal, float maxVal, int color, int xOffset, int isVoltage) {
        spr.setTextColor(color);
        for (int i = 0; i <= 3; i++) {
            float val = maxVal - (maxVal - minVal) * i / 3;
            int y = safeMap(val, minVal, maxVal, graphY + graphHeight, graphY);
            spr.drawString(String(isVoltage ? val / 10 : val, 2), xOffset, y + 2);
        }
    };

    auto drawGrid = [&](float minVal, float maxVal, int isVertical) {
        spr.setTextColor(TFT_CYAN, TFT_BLACK);
        for (int i = 0; i <= 3; i++) {
            if (isVertical) {
                int x = graphX + (graphWidth * i) / 3;
                spr.drawLine(x, graphY, x, graphY + graphHeight, 0x7BCF);
            } else {
                int y = safeMap(minVal + (maxVal - minVal) * i / 3, minVal, maxVal, graphY + graphHeight, graphY);
                spr.drawLine(graphX, y, graphX + graphWidth, y, 0x7BCF);
            }
        }
    };

    // 左侧电压刻度
    drawScale(MinVoltage, MaxVoltage, TFT_GREEN, graphX - 20, 1);

    // 右侧电流刻度
    drawScale(MinCurrent, MaxCurrent, TFT_YELLOW, graphX + graphWidth + 15, 0);

    // 绘制网格线
    drawGrid(MinVoltage, MaxVoltage, 0);
    drawGrid(MinVoltage, MaxVoltage, 1);

    // 绘制曲线
    auto drawCurve = [&](float* data, float minVal, float maxVal, int color) {
        for (int i = 1; i < VNUM_POINTS; i++) {
            int x1 = safeMap(i - 1, 0, VNUM_POINTS - 1, graphX, graphX + graphWidth);
            int y1 = safeMap(data[i - 1], minVal, maxVal, graphY + graphHeight, graphY);
            int x2 = safeMap(i, 0, VNUM_POINTS - 1, graphX, graphX + graphWidth);
            int y2 = safeMap(data[i], minVal, maxVal, graphY + graphHeight, graphY);
            spr.drawLine(x1, y1, x2, y2, color);
        }
    };

    // 绘制电压曲线
    drawCurve(VoltageData, MinVoltage, MaxVoltage, TFT_GREEN);

    // 绘制电流曲线
    drawCurve(CurrentData, MinCurrent, MaxCurrent, TFT_YELLOW);

    // 量程指示
    spr.setTextColor(TFT_GREEN);
    spr.setTextDatum(TL_DATUM);
    spr.drawString("V:" + String(LoadVoltage, 4), 30, 2);

    spr.setTextColor(TFT_YELLOW);
    spr.setTextDatum(TR_DATUM);
    spr.drawString("A:" + String(LoadCurrent, 4), 210, 2);

    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

// 测试界面
void HAL::UI_Page1() {
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(CC_DATUM);
    spr.setColorDepth(8);
    spr.setTextColor(TFT_WHITE);
    spr.loadFont(Font1_12);
    spr.setCursor(2, 10);
    spr.print("Button Test");
    spr.setCursor(2, 30);
    spr.print("Status: " + bottonStatus);

    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

// PD 电源配置界面
void HAL::UI_PowerDelivery(){
    spr.createSprite(240, 240);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(TL_DATUM);
    spr.setColorDepth(8);
    spr.loadFont(Font1_18); // 加载字体

    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

// 快充协议界面
void HAL::UI_QuickCharge(){
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(CC_DATUM);
    spr.setColorDepth(8);
    spr.setTextColor(TFT_WHITE);
    spr.loadFont(Font1_12);
    spr.setCursor(TFT_WIDTH / 2, TFT_HEIGHT / 2);
    spr.print("Quick Charge PAGE!");
    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

//PD 监听界面
const uint8_t MAX_LINES = 1;
const uint8_t LINE_HEIGHT = 180;
const uint8_t START_Y = 1;
String logBuffer[MAX_LINES];
void HAL::UI_LOG(){
    spr.createSprite(240, 240);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(TL_DATUM);
    spr.setColorDepth(8);
    spr.loadFont(Font1_18); // 加载字体
    spr.fillRect(0,0,240,22,0X7EB1); // 绘制顶部边框
    spr.setTextColor(TFT_WHITE);
    spr.setCursor(5, 2); // 设置光标位置
    spr.print("PD Log Monitor"); // 显示标题
    spr.unloadFont();
    spr.loadFont(Font1_12);
    spr.setTextColor(TFT_WHITE);
    static bool firstRun = true; // 用于第一次加载时的特殊处理
    if (firstRun) {
        // 初始化显示缓冲区
        for (int i = 0; i < MAX_LINES; i++)
        {
            logBuffer[i] = "";
            firstRun = false; // 只在第一次运行时执行
        } 
    }
    static long PDtimeMillis = millis(); // 用于记录PD时间
    if (millis() - PDtimeMillis >= 100)
    {
        PDtimeMillis = millis(); // 更新PD时间
        // 更新显示缓冲区
        for (int i = 0; i < MAX_LINES -1; i++)
        {
            logBuffer[i] = logBuffer[i + 1]; // 向上移动一行
        }
        logBuffer[MAX_LINES - 1] = String(pdbuf); // 将最新的PD监听数据放在最后一行
    }
    spr.fillRect(0,24,spr.width(), LINE_HEIGHT * MAX_LINES, TFT_BLACK); // 清除显示区域
    for (int i = 0; i < MAX_LINES; i++)
    {
        // spr.drawString(logBuffer[i], 0, START_Y + i * LINE_HEIGHT); // 绘制每一行
        spr.drawString(logBuffer[i], 0, 24 + i * LINE_HEIGHT, 1); // 绘制每一行
    }

    spr.setTextColor(0XA7FA);
    spr.setCursor(5, 220); // 设置光标位置
    if (PD_Option >= 0 && PD_Option <= 3)
    {
        // spr.print("FIX:" + String(PD_Voltage*0.05,2) + "V " + String(PD_Current*0.01,2) + "A");
        spr.print(" FIX:" + String(PD_Voltage,2) + "V " + String(PD_Current,2) + "A");
    }else if (PD_Option == 1)
    {
        // spr.print("PPS:" + String(PD_Voltage*0.02,2) + "V " + String(PD_Current*0.05,2) + "A");
        spr.print(" PPS:" + String(PD_Voltage,2) + "V " + String(PD_Current,2) + "A");
    }
    spr.print(" SRC:" + String(PD_Src_Cap_Count));
    spr.print(" POS:" + String(PD_Position));
    if (ccbus_used == 0)
    {
        spr.print(" CC:N/A");
    }else if (ccbus_used == 1)
    {
        spr.print(" CC:CC1");
    }else if (ccbus_used == 2)
    {
        spr.print(" CC:CC2");
    }
    spr.setTextColor(TFT_WHITE);
    spr.setCursor(180,10);
    spr.print("FPS:" + String(currentFPS,2));
    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

// 菜单界面
void HAL::UI_Menu(){
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(CC_DATUM);
    spr.setColorDepth(8);
    spr.setTextColor(TFT_WHITE);
    spr.loadFont(Font1_12);

    spr.print("Menu PAGE!");

    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

// 设置界面
void HAL::UI_Setting(){
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(CC_DATUM);
    spr.setColorDepth(8);
    spr.setTextColor(TFT_WHITE);
    spr.loadFont(Font1_18);

    spr.setCursor(2,2);
    spr.print("Default Enable PD: " + String(PD_Enable ? "ON" : "OFF"));
    spr.setCursor(2,22);
    spr.print("LCD Light: " + String(LCD_Light) + "%");

    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

// 系统信息界面
void HAL::UI_SystemInfo(){
    /**/
}

// OTA 更新界面
void HAL::UI_OTA_Update(){
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(MC_DATUM);
    spr.setColorDepth(8);

    spr.setTextColor(TFT_WHITE);
    spr.loadFont(Font1_12);
    spr.setCursor(7,25);
    spr.print("IP: " + String(WiFi.localIP()));
    spr.setCursor(110,25);
    spr.print("HTTP://ESP32.LOCAL");

    spr.setTextColor(TFT_WHITE);
    spr.setCursor(180,10);
    spr.print("FPS:" + String(currentFPS,2));

    spr.unloadFont();
    spr.loadFont(Font1_18);
    spr.setCursor(5,1);
    if (OTA_Progress == 0)
    {
        spr.print("等待更新");
    }else if (OTA_Progress > 0 && OTA_Progress < 100)
    {
        spr.print("更新中");
    }

    spr.setCursor(65,110);
    spr.print("OTA-Update");
    spr.pushImage(96,60,48,48,cloud_download);

    spr.fillRoundRect(18,140,200,6,2,0xffff);
    spr.fillRoundRect(18,140,(OTA_Progress*2),6,2,0x1C9F);
    spr.setCursor(110,150);
    spr.print(OTA_Progress + String("%"));
    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

// OTA 更新完成界面
void HAL::UI_OTA_Finish(){
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(MC_DATUM);
    spr.setColorDepth(8);

    spr.setTextColor(TFT_WHITE);
    spr.loadFont(Font1_18);
    spr.setCursor(5,1);
    spr.print("Finish");

    spr.setCursor(65,110);
    spr.print("OTA-Update");
    spr.pushImage(96,60,48,48,update_success);
    
    spr.fillRoundRect(18,140,200,6,2,0xffff);
    spr.fillRoundRect(18,140,(OTA_Progress*2),6,2,0x1C9F);
    spr.setCursor(80,150);
    spr.setTextColor(TFT_GREEN);
    spr.print("更新完成");
    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

// OTA 更新失败界面
void HAL::UI_OTA_Fail(){
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(MC_DATUM);
    spr.setColorDepth(8);

    spr.setTextColor(TFT_WHITE);
    spr.loadFont(Font1_18);
    spr.setCursor(5,1);
    spr.print("Error");

    spr.setCursor(65,110);
    spr.print("OTA-Update");
    spr.pushImage(96,60,48,48,update_success);
    
    spr.fillRoundRect(18,140,200,6,2,0xffff);
    spr.fillRoundRect(18,140,(OTA_Progress*2),6,2,0x1C9F);
    spr.setCursor(80,150);
    spr.setTextColor(TFT_RED);
    spr.print("更新失败");

    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

// WiFi 连接界面
void HAL::UI_WiFi_Connect(){
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(CC_DATUM);
    spr.setColorDepth(8);

    spr.setTextColor(TFT_WHITE);
    spr.loadFont(Font1_20);
    spr.setCursor(72,110);
    spr.print("连接WiFi...");
    spr.pushImage(96,60,48,48,wlan_error);
    spr.setCursor(10,180);
    spr.print("Connecting...Wait 15s");

    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

// WiFi 连接失败界面
void HAL::UI_WiFi_Connect_Fail(){
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(CC_DATUM);
    spr.setColorDepth(8);

    spr.setTextColor(TFT_WHITE);
    spr.loadFont(Font1_20);
    spr.setCursor(58,110);
    spr.print("连接WiFi失败!");
    spr.pushImage(96,60,48,48,wlan_error);
    spr.setCursor(10,180);
    spr.print("HostAP : ESP32AP");

    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}