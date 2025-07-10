#include "HAL.h"
#include "Library.h"
#include "GlobalVariables.h"
#include "Config.h"

void HAL::GPIO_Init() {
    analogReadResolution(12);
    analogSetClockDiv(1);
    analogSetAttenuation(ADC_11db);

    pinMode(LCD_BL_PIN, OUTPUT);
    pinMode(DP_PIN, INPUT);
    pinMode(DN_PIN, INPUT);
    pinMode(NTC_PIN, INPUT);
    pinMode(SW1, INPUT);
    pinMode(SW2, INPUT);
    pinMode(SW3, INPUT);
    pinMode(SW4, INPUT);
    pinMode(CC1_PIN, INPUT);
    pinMode(CC2_PIN, INPUT);

}

void HAL::GPIO_Run() {
    int ADC_DP = analogRead(DP_PIN);
    int ADC_DN = analogRead(DN_PIN);
    int ADC_CC1 = analogRead(CC1_PIN);
    int ADC_CC2 = analogRead(CC2_PIN);
    
    v_DP = (float)ADC_DP * 3.3 / 4095.0;
    v_DN = (float)ADC_DN * 3.3 / 4095.0;
    v_CC1 = (float)ADC_CC1 * 3.3 / 4095.0;
    v_CC2 = (float)ADC_CC2 * 3.3 / 4095.0;

    NTCv = map(analogRead(NTC_PIN), 0, 4095, 0, 3300);
    NTCm = (3300 - NTCv) * 10000 / NTCv;
    NTC_Temperature = (3450 * 298.15) / (3450 + (298.15 * log(NTCm / 10000))) - 273.15;
    /*
     * R=10K ohm
     * Beta:3450K
     * Kelvins:298.15 = 0(*C)
     * Kelvins:273.15 = 25(*C)
     */
}