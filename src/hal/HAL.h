#ifndef HAL_H
#define HAL_H
#ifdef __cplusplus

#include <stdint.h>
#include "Library.h"
#include "GlobalVariables.h"

namespace HAL
{
    void Sys_Init();
    void Sys_Run();

    float GET_CPU_Temperature();

    void LCD_Init();
    void GPIO_Init();
    void GPIO_Run();
    void INA22x_Init();
    void INA22x_Run();
    void PD_Init();
    void PD_Run();
    void Buzzer_Init();
    void Buzzer_Short();
    void Buzzer_Long();
    void VBUS_Curve();
    void WebUpdate();
    void SaveWiFiConfig();
    void ReadWiFiConfig();
    void DeleteWiFiConfig();
}

#endif // !__cplusplus

#endif // HAL_H