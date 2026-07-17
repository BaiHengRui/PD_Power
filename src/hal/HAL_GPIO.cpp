// ============================================================
// GPIO / ADC reading
// ============================================================
#include "hal.h"

void HAL::GPIO_Init()
{
    pinMode(DN_PIN, INPUT);
    pinMode(DP_PIN, INPUT);
    pinMode(CC1_PIN, INPUT);
    pinMode(CC2_PIN, INPUT);
    pinMode(NTC_PIN, INPUT);
    pinMode(VBUS_ADC, INPUT);
    analogReadResolution(12);
}

void HAL::GPIO_Read()
{
    v_DN  = analogRead(DN_PIN)  * 3.3f / 4095.0f;
    v_DP  = analogRead(DP_PIN)  * 3.3f / 4095.0f;
    v_CC1 = analogRead(CC1_PIN) * 3.3f / 4095.0f;
    v_CC2 = analogRead(CC2_PIN) * 3.3f / 4095.0f;

    float ntcV = analogRead(NTC_PIN) * 3.3f / 4095.0f;
    // 简易 NTC 温度估算 (10kΩ @ 25℃, β=3950)
    if (ntcV > 0.01f && ntcV < 3.29f) {
        float r = 10000.0f * ntcV / (3.3f - ntcV);
        NTC_Temperature = 1.0f / (logf(r / 10000.0f) / 3950.0f + 1.0f / 298.15f) - 273.15f;
    } else {
        NTC_Temperature = 25.0f;
    }
}
