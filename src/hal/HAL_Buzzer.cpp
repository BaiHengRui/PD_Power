#include "HAL.h"
#include "Config.h"

HAL::Buzzer_status_t Buzzer;

void HAL::Buzzer_Init() {
    ledcSetup(0, BUZZER_FREQUENCY, 8);
    ledcAttachPin(BUZZER_PIN, 0);
    Buzzer.BuzzerMute = false;
    HAL::Buzzer_Short(); // beep
}

void HAL::Buzzer_Short() {
    if (Buzzer.BuzzerMute) return;
    ledcWriteTone(0, BUZZER_FREQUENCY);
    delay(40);
    ledcWriteTone(0, 0);
}

void HAL::Buzzer_Long() {
    if (Buzzer.BuzzerMute) return;
    ledcWriteTone(0, BUZZER_FREQUENCY);
    delay(1000);
    ledcWriteTone(0, 0);
}