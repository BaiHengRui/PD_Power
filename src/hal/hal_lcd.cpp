// ============================================================
// LCD / TFT module (TFT_eSPI)
// ============================================================
#include "hal.h"
#include <TFT_eSPI.h>

// tft 定义在 globals.cpp 中, 此处仅引用

void HAL::LCD_Init()
{
    tft.init();
    tft.setRotation(defaultRotation);
    currentRotation = defaultRotation;
    tft.fillScreen(TFT_BLACK);

    HAL::LCD_SetBrightness(defaultBrightness);
    HAL::LOG_INFO("LCD ready (TFT_eSPI)");
}

void HAL::LCD_SetBrightness(uint8_t b)
{
    if (b > 100) b = 100;
    analogWrite(LCD_BL_PIN, map(b, 0, 100, 0, 255));
    defaultBrightness = b;
}

void HAL::LCD_SetRotation(uint8_t r)
{
    if (r > 3) r = 0;
    pendingRotation = r;
}

void HAL::LCD_Refresh(uint32_t bgColor)
{
    // 帧间安全切换旋转
    if (pendingRotation >= 0) {
        tft.setRotation((uint8_t)pendingRotation);
        currentRotation = (uint8_t)pendingRotation;
        pendingRotation = -1;
    }
    tft.fillScreen(bgColor);
}

void HAL::LCD_Backlight(bool on)
{
    digitalWrite(LCD_BL_PIN, on ? HIGH : LOW);
}

void HAL::LCD_Light_Updat(int light, bool saved)
{
    light = constrain(light, 1, 100);
    lcdBrightness = light;
    int pwm = 255 - ((100 - light) * 1.5);
    analogWrite(LCD_BL_PIN, pwm);
    if (saved) { HAL::NVS_WriteUChar("light", light); }
}

void HAL::LCD_Rotation_Update(int rotation, bool saved)
{
    LCD_Rotation = constrain(rotation, 0, 3);
    tft.setRotation(LCD_Rotation);
    if (saved) { HAL::NVS_WriteUChar("rotation", rotation); }
}
