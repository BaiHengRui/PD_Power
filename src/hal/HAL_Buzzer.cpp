// ============================================================
// Buzzer module
// ============================================================
#include "hal.h"

void HAL::Buzzer_Init()
{
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
}

void HAL::Buzzer_Beep(uint16_t ms)
{
    if (Buzzer.mute) return;
    tone(BUZZER_PIN, BUZZER_FREQ, ms);
}

void HAL::Buzzer_SetMute(bool mute)
{
    Buzzer.mute = mute;
}
