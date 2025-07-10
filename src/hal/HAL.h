#ifndef HAL_H
#define HAL_H
#ifdef __cplusplus

#include <stdint.h>
#include "Library.h"
#include "GlobalVariables.h"

namespace HAL
{
    /* system */
    void Sys_Init();
    void Sys_Run();
    void WebUpdate();
    void SaveWiFiConfig();
    void ReadWiFiConfig();
    void DeleteWiFiConfig();

    /* gpio */
    void GPIO_Init();
    void GPIO_Run();

    /* ina */
    void INA22x_Init();
    void INA22x_Run();

    /*pd */
    void PD_Init();
    void PD_Run();

    /* buzzer */
    void Buzzer_Init();
    void Buzzer_Short();
    void Buzzer_Long();

    /* ui */
    void LCD_Init();
    void UI_Main();
    void UI_VBUS_Curve();
    void UI_PowerDelivery();
    void UI_LOG();

    typedef struct
    {
        bool BuzzerMute; // Buzzer mute status
    } Buzzer_status_t;

    struct config_type
    {
        char ssid[32]; // 配网WiFi的SSID
        char psw[64];  // 配网WiFi的Password
    };config_type wificonf = {{""}, {""}};

}


#endif // !__cplusplus

#endif // HAL_H