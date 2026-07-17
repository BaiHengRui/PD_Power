#include "ui.h"
#include "../hal/hal.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include "../assets/fonts/Font1_12.h"
#include "../assets/fonts/Font1_18.h"
#include "../assets/fonts/Font1_20.h"
#include "../assets/fonts/Font1_40.h"
#include "../assets/imgs/cloud_download.h"
#include "../assets/imgs/update_error.h"
#include "../assets/imgs/update_success.h"
#include "../assets/imgs/wlan_error.h"

// tft is now in globals.cpp
TFT_eSprite spr = TFT_eSprite(&tft);

// Clear the sprite canvas (called before each page redraw)
void HAL::LCD_Refresh_Screen(uint32_t bgcolor){
    spr.fillScreen(bgcolor);
}

// LCD Init/Light/Rotation now in hal_lcd.cpp

// Main page display
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
    spr.print(INA.voltage, INA.voltage < 10 ? 5 : 4);
    spr.setCursor(140,5);
    spr.print("V");

    spr.setCursor(5,52); 
    spr.setTextColor(TFT_YELLOW);
    spr.print(INA.current, INA.current < 10 ? 5 : 4);
    spr.setCursor(140,52);
    spr.print("A");

    spr.setCursor(5,97); 
    spr.setTextColor(TFT_CYAN);
    spr.print(INA.power, INA.power < 10 ? 5 : (INA.power < 100 ? 4 : 3));
    spr.setCursor(140,97);
    spr.print("W");
    spr.unloadFont();

    spr.loadFont(Font1_12);
    spr.setCursor(0, 150);
    spr.setTextColor(TFT_YELLOW);
    spr.println("  SET:");
    if (!PD.is_pps)
    {
        spr.println("  FIX: " + String(PD.voltage, 2) + "V " + String(PD.current, 2) + "A");
    }else if (PD.is_pps)
    {
        spr.println("  PPS: " + String(PD.voltage, 2) + "V " + String(PD.current, 2) + "A");
    }
    spr.print("  SRC:" + String(PD.pdo_count));
    spr.print("  POS:" + String(PD.selected_pos));

    spr.setCursor(180,10);
    spr.setTextColor(TFT_WHITE);
    spr.print("FPS:" + String(currentFPS,2));
    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

// VBUS 电压电流曲线 (240x240 方屏)
void HAL::UI_VBUS_Curve() {
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(0x0000);

    if (!graphDataInitialized) {
        spr.loadFont(Font1_12);
        spr.setTextColor(0xFFFF);
        spr.setTextDatum(CC_DATUM);
        spr.drawString("Sampling...", 120, 120);
        spr.unloadFont();
        spr.pushSprite(0, 0);
        spr.deleteSprite();
        return;
    }

    // --- 布局: 双行信息头(30px) + 下方满幅曲线(210px) ---
    const int HEADER_H = 30;
    const int graphX = 0;
    const int graphY = HEADER_H;
    const int graphW = 240;
    const int graphH = TFT_HEIGHT - HEADER_H;  // 210px
    const int gridCols = 5;
    const int gridRows = 3;
    const uint16_t gridColor = 0x39E7;

    float dv = graphPaused ? frozenVoltage : INA.voltage;
    float di = graphPaused ? frozenCurrent : INA.current;

    // --- 防除零 ---
    if (vDisplayMax <= vDisplayMin) vDisplayMax = vDisplayMin + 0.1f;
    if (iDisplayMax <= iDisplayMin) iDisplayMax = iDisplayMin + 0.1f;

    // --- 绘制网格 ---
    for (int col = 0; col <= gridCols; col++) {
        int x = graphX + (col * (graphW - 1)) / gridCols;
        spr.drawLine(x, graphY, x, graphY + graphH - 1, gridColor);
    }
    for (int row = 0; row <= gridRows; row++) {
        int y = graphY + (row * (graphH - 1)) / gridRows;
        spr.drawLine(graphX, y, graphX + graphW - 1, y, gridColor);
    }

    // --- 绘制电压曲线 (绿色) ---
    for (int i = 1; i < graphW; i++) {
        int x1 = graphX + i - 1, x2 = graphX + i;
        float val1 = voltageBuffer[(graphIndex + i - 1) % GRAPH_WIDTH];
        float val2 = voltageBuffer[(graphIndex + i) % GRAPH_WIDTH];
        val1 = fmaxf(vDisplayMin, fminf(val1, vDisplayMax));
        val2 = fmaxf(vDisplayMin, fminf(val2, vDisplayMax));
        int y1 = graphY + graphH - 1 - (int)((val1 - vDisplayMin) * (graphH - 1) / (vDisplayMax - vDisplayMin));
        int y2 = graphY + graphH - 1 - (int)((val2 - vDisplayMin) * (graphH - 1) / (vDisplayMax - vDisplayMin));
        spr.drawLine(x1, y1, x2, y2, 0x2EA6);
    }

    // --- 绘制电流曲线 (黄色) ---
    for (int i = 1; i < graphW; i++) {
        int x1 = graphX + i - 1, x2 = graphX + i;
        float val1 = currentBuffer[(graphIndex + i - 1) % GRAPH_WIDTH];
        float val2 = currentBuffer[(graphIndex + i) % GRAPH_WIDTH];
        val1 = fmaxf(iDisplayMin, fminf(val1, iDisplayMax));
        val2 = fmaxf(iDisplayMin, fminf(val2, iDisplayMax));
        int y1 = graphY + graphH - 1 - (int)((val1 - iDisplayMin) * (graphH - 1) / (iDisplayMax - iDisplayMin));
        int y2 = graphY + graphH - 1 - (int)((val2 - iDisplayMin) * (graphH - 1) / (iDisplayMax - iDisplayMin));
        spr.drawLine(x1, y1, x2, y2, 0xFEE0);
    }

    // --- 文字叠加 (Font1_12) ---
    spr.loadFont(Font1_12);
    char buf[24];

    // 第1行 (y=2):  Now V | Max V | ON/OFF
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(0x2EA6);
    sprintf(buf, "Now:%.2fV", dv);
    spr.drawString(buf, 2, 2);

    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(0x2EA6);
    sprintf(buf, "Max:%.2fV", vHistoryMax);
    spr.drawString(buf, 120, 2);

    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(graphPaused ? 0xF800 : 0x07FF);
    spr.drawString(graphPaused ? "OFF" : "ON", 238, 2);

    // 第2行 (y=17): Now I | Min I | s/div
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(0xFEE0);
    sprintf(buf, "Now:%.2fA", di);
    spr.drawString(buf, 2, 17);

    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(0xFEE0);
    sprintf(buf, "Min:%.2fA", iDisplayMin);
    spr.drawString(buf, 120, 17);

    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(0x8410);
    spr.drawString("1.35s/d", 238, 17);

    // --- 网格内叠加: 顶部=最大值, 底部=最小值 ---
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(0xD69A);
    sprintf(buf, "%.2fV/%.2fA", vDisplayMax, iDisplayMax);
    spr.drawString(buf, 2, graphY + 2);
    spr.setTextDatum(BL_DATUM);
    sprintf(buf, "%.2fV/%.2fA", vDisplayMin, iDisplayMin);
    spr.drawString(buf, 2, graphY + graphH - 2);

    // PAUSED 水印
    if (graphPaused) {
        spr.setTextDatum(CC_DATUM);
        spr.setTextColor(0xF800);
        spr.drawString("PAUSED", graphW/2, graphY + graphH/2);
    }

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
    spr.print("Status: " + btnStatus);

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

    // 直接显示 logLine (由 PD 任务实时更新)
    const char* pLine = logLine[0] ? logLine : "Waiting PD...";
    spr.drawString(pLine, 5, 26, 1);

    spr.setTextColor(0XA7FA);
    spr.setCursor(5, 220); // 设置光标位置
    if (PD.is_pps)
    {
        spr.print(" PPS:" + String(PD.voltage,2) + "V " + String(PD.current,2) + "A");
    }else if (PD.connected)
    {
        spr.print(" FIX:" + String(PD.voltage,2) + "V " + String(PD.current,2) + "A");
    }
    spr.print(" SRC:" + String(PD.pdo_count));
    spr.print(" POS:" + String(PD.selected_pos));
    if (PD.cc_pin == 0)
    {
        spr.print(" CC:N/A");
    }else if (PD.cc_pin == 1)
    {
        spr.print(" CC:CC1");
    }else if (PD.cc_pin == 2)
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
    spr.print("LCD Light: " + String(lcdBrightness) + "%");

    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

// 系统信息界面
void HAL::UI_SystemInfo(){
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(TL_DATUM);
    spr.setColorDepth(8);
    spr.setTextColor(TFT_WHITE);
    spr.loadFont(Font1_12);

    spr.setCursor(2, 2);
    spr.println("System Info");
    spr.println("SW: " + String(SOFTWARE_VERSION));
    spr.println("HW: " + String(HARDWARE_VERSION));
    spr.println("SN: " + String((uint32_t)(SNID >> 32), HEX) + String((uint32_t)SNID, HEX));
    spr.println("Uptime: " + HAL::Get_System_RunTime(micros()));
    spr.print("CPU Temp: " + String(HAL::Get_CPU_Temperature(), 1) + "C");

    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
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
    spr.print("IP: " + WiFi.localIP().toString());
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
        spr.print("Updating...");
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
    spr.print("AP: ESP32AP");

    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}
void HAL::UI_Run()
{
    static uint8_t prev = 0xFF;
    if (pendingRotation >= 0) {
        tft.setRotation((uint8_t)pendingRotation);
        currentRotation = (uint8_t)pendingRotation;
        prev = 0xFF; pendingRotation = -1;
    }
    if (nowApp != prev) { prev = nowApp; tft.fillScreen(TFT_BLACK); }
    switch (nowApp) {
        case 0: UI_Main(); break;
        case 1: UI_VBUS_Curve(); break;
        case 2: UI_Page1(); break;
        case 3: UI_Menu(); break;
        case 4: UI_Setting(); break;
        case 5: UI_SystemInfo(); break;
        case 6: UI_PowerDelivery(); break;
        case 7: UI_QuickCharge(); break;
        case 8: UI_LOG(); break;
        case 9: UI_WiFi_Connect(); break;
        case 10: UI_WiFi_Connect_Fail(); break;
        case 11: UI_OTA_Update(); break;
        case 12: UI_OTA_Finish(); break;
        case 13: UI_OTA_Fail(); break;
        default: UI_Main(); break;
    }
    vTaskDelay(1);  // 主动让出 CPU，防止看门狗超时
}