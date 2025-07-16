#include "HAL.h"
#include "Config.h"
#include "Fonts/Font1_12.h"
#include "Fonts/Font1_18.h"
#include "Fonts/Font1_20.h"
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
float voltageMax = 10;    // 初始值10V
float voltageMin = 0;
float currentMax = 1;     // 初始值1A
float currentMin = 0;;
float powerMax = 50;
float powerMin = 0;

void HAL::LCD_Init(){
    // analogWrite(LCD_BL_PIN,0);
    spr.init();
    spr.invertDisplay(LCD_InvertDisplay);
    spr.setRotation(LCD_Rotation);

    int Light_temp = EEPROM.read(EEPROM_Light_addr);
    int Rotation_temp = EEPROM.read(EEPROM_Rotation_addr);
    LCD_Light = (Light_temp >=1 && Light_temp <=100) ? Light_temp : LCD_Light;
    LCD_Rotation = (Rotation_temp >=0 && Rotation_temp <= 3) ? Rotation_temp : LCD_Rotation;
    if(Light_temp >=1 && Light_temp <=100){EEPROM.write(EEPROM_Light_addr,LCD_Light);EEPROM.commit();}
    if(Rotation_temp >=0 && Rotation_temp <=3){EEPROM.write(EEPROM_Rotation_addr,LCD_Rotation);EEPROM.commit();}
}

void HAL::LCD_Light_Updat(int light, bool saved){
    light = constrain(light,1,100);//限制大小
    int light_pwm = 255 - ((100 - light) * 1.5);
    analogWrite(LCD_BL_PIN,light_pwm);
    if(saved == 1){EEPROM.write(EEPROM_Light_addr,light);EEPROM.commit();}
}

void HAL::LCD_Rotation_Update(int rotation, bool saved){
    LCD_Rotation = constrain(rotation,0,3);
    tft.setRotation(LCD_Rotation);
    if(saved == 1){EEPROM.write(EEPROM_Rotation_addr,LCD_Rotation);EEPROM.commit();}
}

void HAL::LCD_Refresh_Screen(uint32_t bgcolor){
    spr.fillScreen(bgcolor);
}

void HAL::UI_Main(){
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(CC_DATUM);
    spr.setColorDepth(8);
    spr.setTextColor(TFT_WHITE);
    spr.loadFont(Font1_20);
    spr.setCursor(2,10);
    spr.setTextColor(TFT_GREEN);
    spr.print(LoadVoltage,4);
    spr.print("Vbus");
    spr.setCursor(2,35);
    spr.setTextColor(TFT_CYAN);
    spr.print(LoadCurrent,4);
    spr.print("Ibus");
    spr.setCursor(2,60);
    spr.setTextColor(TFT_YELLOW);
    spr.print(LoadPower,4);
    spr.print("Power");

    spr.unloadFont();
    spr.loadFont(Font1_12);
    spr.setCursor(2,120);
    if (PD_Option == 0)
    {
        spr.print("FIX:" + String(PD_Voltage*0.05,2) + "V " + String(PD_Current*0.01,2) + "A");
    }else if (PD_Option == 1)
    {
        spr.print("PPS:" + String(PD_Voltage*0.02,2) + "V " + String(PD_Current*0.05,2) + "A");
    }
    spr.setCursor(2,140);
    spr.print("SRC:" + String(PD_Src_Cap_Count));
    spr.print("-POS:" + String(PD_Position));

    spr.setTextColor(TFT_WHITE);
    spr.setCursor(180,10);
    spr.print("FPS:" + String(currentFPS,2));
    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

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
        voltageMin = (1 - smoothFactor) * voltageMin + smoothFactor * (center - 0.5f);
        voltageMax = (1 - smoothFactor) * voltageMax + smoothFactor * (center + 0.5f);
    } else {
        voltageMax = (1 - smoothFactor) * voltageMax + smoothFactor * (vMax + vRange * 0.1f);
        voltageMin = (1 - smoothFactor) * voltageMin + smoothFactor * (vMin - vRange * 0.1f);
    }
    voltageMin = std::max(voltageMin, 0.0f);

    // 电流量程调整
    float cRange = cMax - cMin;
    if (cRange < 0.1f) {
        float center = (cMax + cMin) / 2;
        currentMin = (1 - smoothFactor) * currentMin + smoothFactor * (center - 0.05f);
        currentMax = (1 - smoothFactor) * currentMax + smoothFactor * (center + 0.05f);
    } else {
        currentMax = (1 - smoothFactor) * currentMax + smoothFactor * (cMax + cRange * 0.1f);
        currentMin = (1 - smoothFactor) * currentMin + smoothFactor * (cMin - cRange * 0.1f);
    }
    currentMin = std::max(currentMin, 0.0f);
    if (currentMax - currentMin < 0.1f) currentMax = currentMin + 0.1f;

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
    drawScale(voltageMin, voltageMax, TFT_GREEN, graphX - 20, 1);

    // 右侧电流刻度
    drawScale(currentMin, currentMax, TFT_YELLOW, graphX + graphWidth + 15, 0);

    // 绘制网格线
    drawGrid(voltageMin, voltageMax, 0);
    drawGrid(voltageMin, voltageMax, 1);

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
    drawCurve(VoltageData, voltageMin, voltageMax, TFT_GREEN);

    // 绘制电流曲线
    drawCurve(CurrentData, currentMin, currentMax, TFT_YELLOW);

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


// 趋势检测相关全局变量
enum VoltageTrend { STABLE, DECREASING, INCREASING };
static VoltageTrend voltageTrend = STABLE;
static unsigned long trendStartTime = 0;
static float initialDecreaseMin = 0.0f;  // 进入下降状态时的初始最小值
static float initialIncreaseMax = 0.0f;  // 进入上升状态时的初始最大值
static float lastStableMin = 0.0f;       // 稳定状态下的参考最小值
static float lastStableMax = 0.0f;       // 稳定状态下的参考最大值
const unsigned long TREND_DELAY_MS = 2000; // 下降延迟调整时间（2秒）
const float TREND_THRESHOLD = 0.05f;     // 趋势判断阈值（50mV，可调整）

void HAL::UI_VBUS_Waveform() {
    const int GRAPH_X = 30;          // 左侧边距
    const int GRAPH_Y = 30;          // 顶部边距
    const int GRAPH_WIDTH = 180;     // 绘图区宽度
    const int GRAPH_HEIGHT = 160;    // 绘图区高度

    // 计算中央位置用于坐标轴
    const int GRAPH_CENTER_X = GRAPH_X + GRAPH_WIDTH / 2;
    const int GRAPH_CENTER_Y = GRAPH_Y + GRAPH_HEIGHT / 2;

    // 安全映射函数（保持在函数内部）
    auto safeMap = [](float value, float inMin, float inMax, int outMin, int outMax) {
        if (inMin >= inMax) return (outMin + outMax) / 2;
        return (int)((value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin);
    };

    // 安全获取极值函数（保持在函数内部）
    auto safeExtremes = [](float* data, int len) -> std::pair<float, float> {
        if (len == 0) return {0, 0};
        float minVal = data[0], maxVal = data[0];
        for (int i = 1; i < len; ++i) {
            if (data[i] < minVal) minVal = data[i];
            if (data[i] > maxVal) maxVal = data[i];
        }
        return {minVal, maxVal};
    };

    // 创建精灵
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(CC_DATUM);
    spr.setTextColor(TFT_WHITE);
    spr.loadFont(Font1_12);

    // 将ADC采样值转换为电压值
    float voltageData[BUFFER_SIZE];
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        voltageData[i] = adcBuffer[i] * (3.3 / 4095);
    }

    // 获取极值
    std::pair<float, float> vExtremes = safeExtremes(voltageData, BUFFER_SIZE);
    float vMin = vExtremes.first;
    float vMax = vExtremes.second;

    // 计算数据波动范围
    float vRange = vMax - vMin;
    
    // 趋势检测与状态机（核心优化部分）
    bool shouldAdjustRange = false;
    
    if (voltageTrend == DECREASING) {
        // 已处于下降状态：检查是否继续下降
        if (vMin < initialDecreaseMin - TREND_THRESHOLD) {
            // 持续下降超过阈值：检查是否达到延迟时间
            if (millis() - trendStartTime >= TREND_DELAY_MS) {
                shouldAdjustRange = true;
                lastStableMin = vMin;
                lastStableMax = vMax;
            }
        } else {
            // 下降趋势中断，切换回稳定状态
            voltageTrend = STABLE;
            lastStableMin = vMin;
            lastStableMax = vMax;
        }
    } else if (voltageTrend == INCREASING) {
        // 已处于上升状态：检查是否继续上升
        if (vMax > initialIncreaseMax + TREND_THRESHOLD) {
            shouldAdjustRange = true;  // 上升立即调整
            lastStableMin = vMin;
            lastStableMax = vMax;
        } else {
            // 上升趋势中断，切换回稳定状态
            voltageTrend = STABLE;
            lastStableMin = vMin;
            lastStableMax = vMax;
        }
    } else {
        // 当前处于稳定状态：检查是否进入新趋势
        if (vMin < lastStableMin - TREND_THRESHOLD) {
            // 首次检测到显著下降
            voltageTrend = DECREASING;
            trendStartTime = millis();
            initialDecreaseMin = lastStableMin;
        } else if (vMax > lastStableMax + TREND_THRESHOLD) {
            // 首次检测到显著上升
            voltageTrend = INCREASING;
            initialIncreaseMax = lastStableMax;
            shouldAdjustRange = true;  // 上升立即调整
        } else {
            // 保持稳定状态：仅当波动范围明显变化时调整
            if (vRange > (lastStableMax - lastStableMin) * 1.5f) {
                shouldAdjustRange = true;
                lastStableMin = vMin;
                lastStableMax = vMax;
            }
        }
    }

    // 电压量程调整（仅在shouldAdjustRange为true时执行）
    if (shouldAdjustRange) {
        if (vRange < 0.1f) {  // 如果波动小于0.1V
            float center = (vMax + vMin) / 2;  // 计算中心值
            voltageMin = center - 0.05f;        // 保持±0.05V的范围
            voltageMax = center + 0.05f;
        } else {
            // 正常自动调整逻辑
            voltageMax = vMax + vRange * 0.1f;
            voltageMin = vMin - vRange * 0.1f;
        }
        voltageMin = std::max(voltageMin, 0.0f); // 电压不低于0
    }

    // 左侧电压刻度
    spr.setTextColor(TFT_GREEN, TFT_BLACK);
    for (int i = 0; i <= 3; i++) {
        float val = voltageMax - (voltageMax - voltageMin) * i / 3;
        int y = safeMap(val, voltageMin, voltageMax, GRAPH_Y + GRAPH_HEIGHT, GRAPH_Y);
        spr.drawString(String(val, 2) + "V", GRAPH_X - 15, y); // 调整水平位置
    }

    // 底部时间刻度
    spr.setTextColor(TFT_YELLOW, TFT_BLACK);
    for (int i = 0; i <= 4; i++) {
        int x = GRAPH_X + (GRAPH_WIDTH * i) / 4;
        spr.drawString(String(i * 0.5 * timeScale, 1) + "ms", x, GRAPH_Y + GRAPH_HEIGHT + 15);
    }

    // 绘制网格线
    spr.setTextColor(TFT_CYAN, TFT_BLACK);
    for (int i = 0; i <= 3; i++) {
        int y = safeMap(voltageMin + (voltageMax - voltageMin) * i / 3,
                        voltageMin, voltageMax, GRAPH_Y + GRAPH_HEIGHT, GRAPH_Y);
        spr.drawLine(GRAPH_X, y, GRAPH_X + GRAPH_WIDTH, y, 0x7BCF);
    }

    for (int i = 0; i <= 4; i++) {
        int x = GRAPH_X + (GRAPH_WIDTH * i) / 4;
        spr.drawLine(x, GRAPH_Y, x, GRAPH_Y + GRAPH_HEIGHT, 0x7BCF);
    }

    // 绘制坐标轴
    spr.drawRect(GRAPH_X, GRAPH_Y, GRAPH_WIDTH, GRAPH_HEIGHT, TFT_WHITE);
    spr.drawFastVLine(GRAPH_CENTER_X, GRAPH_Y, GRAPH_HEIGHT, TFT_WHITE); // 使用预计算的中心
    spr.drawFastHLine(GRAPH_X, GRAPH_CENTER_Y, GRAPH_WIDTH, TFT_WHITE);   // 使用预计算的中心

    // 显示参数信息和趋势状态
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.setTextDatum(TL_DATUM); // 左上对齐
    spr.setCursor(10, 10);
    spr.printf("V:%.1f T:%.1f ", voltageScale, timeScale);
    
    // 显示趋势状态
    spr.setTextColor(voltageTrend == DECREASING ? TFT_RED : 
                    (voltageTrend == INCREASING ? TFT_GREEN : TFT_WHITE));
    switch(voltageTrend) {
        case STABLE: spr.print("STABLE"); break;
        case DECREASING: spr.print("DOWN "); break;
        case INCREASING: spr.print("UP   "); break;
    }
    
    // 显示延迟倒计时
    if (voltageTrend == DECREASING && !shouldAdjustRange) {
        spr.setTextColor(TFT_YELLOW);
        spr.setCursor(120, 10);
        spr.printf("Adjust in: %.1fs", (TREND_DELAY_MS - (millis() - trendStartTime)) / 1000.0f);
    }

    // 绘制波形
    if (adcBuffer) {
        int prevX = GRAPH_X;
        int prevY = safeMap(voltageData[0], voltageMin, voltageMax, GRAPH_Y + GRAPH_HEIGHT, GRAPH_Y);

        // 计算显示的采样点数（基于时间缩放）
        int visibleSamples = std::min(BUFFER_SIZE, (int)(BUFFER_SIZE * timeScale));

        for (int i = 1; i < visibleSamples; i++) {
            int x = GRAPH_X + map(i, 0, visibleSamples, 0, GRAPH_WIDTH);
            int y = safeMap(voltageData[i], voltageMin, voltageMax, GRAPH_Y + GRAPH_HEIGHT, GRAPH_Y);

            // 约束坐标
            x = constrain(x, GRAPH_X, GRAPH_X + GRAPH_WIDTH);
            y = constrain(y, GRAPH_Y, GRAPH_Y + GRAPH_HEIGHT);

            // 绘制线段
            spr.drawLine(prevX, prevY, x, y, TFT_CYAN);

            prevX = x;
            prevY = y;
        }
    }

    // 显示时间戳（调试用）
    spr.setTextDatum(BL_DATUM); // 左下对齐
    spr.setTextColor(TFT_WHITE);
    spr.setCursor(10, 230);
    spr.print(millis());

    spr.unloadFont();
    spr.pushSprite(0, 0);  // 将精灵内容推送到屏幕
    spr.deleteSprite();    // 释放精灵内存
}    

void HAL::UI_PowerDelivery(){
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(CC_DATUM);
    spr.setColorDepth(8);
    spr.setTextColor(TFT_WHITE);
    spr.loadFont(Font1_12);
    spr.setCursor(TFT_WIDTH / 2, TFT_HEIGHT / 2);
    spr.print("Power Delivery PAGE !");
    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

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

void HAL::UI_LOG(){
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(CC_DATUM);
    spr.setColorDepth(8);
    spr.setTextColor(TFT_WHITE);
    spr.loadFont(Font1_12);

    spr.setCursor(TFT_WIDTH / 2, TFT_HEIGHT / 2);
    spr.print("LOG PAGE!");

    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

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

void HAL::UI_Setting(){
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(CC_DATUM);
    spr.setColorDepth(8);
    spr.setTextColor(TFT_WHITE);
    spr.loadFont(Font1_12);

    spr.setCursor(TFT_WIDTH / 2, TFT_HEIGHT / 2);
    spr.print("Setting PAGE!");

    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
}

void HAL::UI_SystemInfo(){
    /**/
}

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