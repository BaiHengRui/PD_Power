#include "HAL.h"
#include "GlobalVariables.h"
#include "Config.h"
#include "Fonts/Font1_12.h"

TFT_eSPI tft = TFT_eSPI(240,240);
TFT_eSprite spr = TFT_eSprite(&tft);

#define VNUM_POINTS 160  //VBUS 与格子宽度一致
float VoltageData[VNUM_POINTS] = {0};
float CurrentData[VNUM_POINTS] = {0};
float PowerData[VNUM_POINTS] = {0};
float voltageMax = 10;    // 初始值10V
float voltageMin = 0;
float currentMax = 1;     // 初始值1A
float currentMin = 0;;
float powerMax = 50;
float powerMin = 0;

void HAL::LCD_Init(){
    analogWrite(LCD_BL_PIN,0);
    spr.init();
    spr.invertDisplay(LCD_InvertDisplay);
    spr.setRotation(LCD_Rotation);

    int light_temp = EEPROM.read(EEPROM_Light_addr);
    int rotation_temp = EEPROM.read(EEPROM_Rotation_addr);
    LCD_Light = (light_temp >=1 && light_temp <=100) ? light_temp : LCD_Light;
    LCD_Rotation = (rotation_temp >=0 && rotation_temp <= 3) ? rotation_temp : LCD_Rotation;

}

void HAL::UI_Main(){
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(CC_DATUM);
    spr.setColorDepth(8);
    spr.setTextColor(TFT_WHITE);
    // spr.loadFont();

    // spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

void HAL::UI_VBUS_Curve(){
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

    // 计算数据波动范围
    float vRange = vMax - vMin;
    float cRange = cMax - cMin;

    // 电压量程调整
    if (vRange < 1.0f) {  // 如果波动小于1.0
        float center = (vMax + vMin) / 2;  // 计算中心值
        voltageMin = center - 0.5f;        // 保持±0.5的范围
        voltageMax = center + 0.5f;
    } else {
        // 正常自动调整逻辑
        voltageMax = vMax + vRange * 0.1f;
        voltageMin = vMin - vRange * 0.1f;
    }
    voltageMin = std::max(voltageMin, 0.0f); // 电压不低于0
    // 电流量程调整

    if (cRange < 0.1f) {
        float center = (cMax + cMin) / 2;
        currentMin = center - 0.05f;
        currentMax = center + 0.05f;
    } else {
        currentMax = cMax + cRange * 0.1f;
        currentMin = cMin - cRange * 0.1f;
    }

    currentMin = std::max(currentMin, 0.0f);
    if (currentMax - currentMin < 0.1f) currentMax = currentMin + 0.1f;
    // 安全映射函数
    auto safeMap = [](float value, float inMin, float inMax, int outMin, int outMax) {
        if (inMin >= inMax) return (outMin + outMax) / 2; // 防除零保护
        return (int)((value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin);
    };

    // 绘制区域参数
    const int graphX = 40;      // 左侧留空40px
    const int graphY = 40;      // 顶部留空40px
    const int graphWidth = 160; // 绘图区宽度
    const int graphHeight = 160;// 绘图区高度

    spr.loadFont(Font1_12);
    spr.setTextColor(TFT_WHITE);

    // 左侧电压刻度
    spr.setTextColor(TFT_GREEN);
    for (int i = 0; i <= 3; i++) {
        float val = voltageMax - (voltageMax - voltageMin) * i / 3;
        int y = safeMap(val, voltageMin, voltageMax, graphY + graphHeight, graphY);
        spr.drawString(String(val/10,2), graphX-20, y+2); // 左侧显示电压值
    }

    // 右侧电流刻度
    spr.setTextColor(TFT_YELLOW);
    for (int i = 0; i <= 3; i++) {
        float val = currentMax - (currentMax - currentMin) * i / 3;
        int y = safeMap(val, currentMin, currentMax, graphY + graphHeight, graphY);
        spr.drawString(String(val/10,2), graphX + graphWidth + 15, y+2); // 右侧显示电流值
    }

    // 绘制网格线
    for (int i = 0; i <= 3; i++) {
        int y = safeMap(voltageMin + (voltageMax-voltageMin)*i/3, 
                      voltageMin, voltageMax, graphY+graphHeight, graphY);
        spr.drawLine(graphX, y, graphX+graphWidth, y, 0x7BCF);
    }
    for (int i = 0; i <= 3; i++) {
        int x = graphX + (graphWidth * i) / 3;
        spr.drawLine(x, graphY, x, graphY + graphHeight, 0x7BCF);
    }

    // 绘制曲线
    for (int i = 1; i < VNUM_POINTS; i++) {
        // 电压
        int x1 = safeMap(i-1, 0, VNUM_POINTS-1, graphX, graphX+graphWidth);
        int y1 = safeMap(VoltageData[i-1], voltageMin, voltageMax, graphY+graphHeight, graphY);
        int x2 = safeMap(i, 0, VNUM_POINTS-1, graphX, graphX+graphWidth);
        int y2 = safeMap(VoltageData[i], voltageMin, voltageMax, graphY+graphHeight, graphY);
        spr.drawLine(x1, y1, x2, y2, TFT_GREEN);

        // 电流
        int cy1 = safeMap(CurrentData[i-1], currentMin, currentMax, graphY+graphHeight, graphY);
        int cy2 = safeMap(CurrentData[i], currentMin, currentMax, graphY+graphHeight, graphY);
        spr.drawLine(x1, cy1, x2, cy2, TFT_YELLOW);
    }

    // 量程指示
    spr.setTextColor(TFT_GREEN);
    spr.setTextDatum(TL_DATUM); // 左对齐
    spr.drawString("V:" + String(LoadVoltage,4), 30, 2);

    spr.setTextColor(TFT_YELLOW);
    spr.setTextDatum(TR_DATUM); // 右对齐
    spr.drawString("A:" + String(LoadCurrent,4), 210, 2); 

    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

void HAL::UI_PowerDelivery(){
    /**/
}

void HAL::UI_QuickCharge(){
    /**/
}

void HAL::UI_LOG(){
    /**/
}

void HAL::UI_Menu(){
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(CC_DATUM);
    spr.setColorDepth(8);
    spr.setTextColor(TFT_WHITE);

    /*code  */

    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

void HAL::UI_Setting(){
    /**/
}

void HAL::UI_SystemInfo(){
    /**/
}

void HAL::UI_OTA_Update(){
    /**/
}

void HAL::UI_OTA_Finish(){
    /**/
}

void HAL::UI_OTA_Fail(){
    /**/
}

void HAL::UI_WiFi_Connect(){
    /**/
}

void HAL::UI_WiFi_Connect_Fail(){
    /**/
}