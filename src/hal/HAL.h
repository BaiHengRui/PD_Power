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
    void WiFiConnect();
    void WebUpdate();
    void WebUptadeRun();
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
    void LCD_Light_Updat(int light, bool saved);
    void LCD_Rotation_Update(int rotation, bool saved);
    void LCD_Refresh_Screen(uint32_t bgcolor =0x0000);
    void UI_Main();
    void UI_VBUS_Curve();
    void UI_PowerDelivery();
    void UI_QuickCharge();
    void UI_LOG();
    void UI_Menu();
    void UI_Setting();
    void UI_SystemInfo();
    void UI_OTA_Update();
    void UI_OTA_Finish();
    void UI_OTA_Fail();
    void UI_WiFi_Connect();
    void UI_WiFi_Connect_Fail();

    typedef struct
    {
        bool BuzzerMute; // Buzzer mute status
    } Buzzer_status_t;

    typedef struct 
    {
        bool OTARun; // OTA Run Status
    } OTA_status_t;

}

namespace AppState {
    constexpr int Main = 1;
    constexpr int VBUS_Curve = 2;
    constexpr int Menu = 3;
    constexpr int Log = 4;
    constexpr int PowerDelivery = 5;
    constexpr int QuickCharge = 6;
    constexpr int SystemInfo = 7;
    constexpr int Setting = 8;
    constexpr int WiFi_Connect = 9;
    constexpr int WiFi_Connect_Fail = 10;
    constexpr int OTA_Update = 11;
    constexpr int OTA_Finish = 12;
    constexpr int OTA_Fail = 13;
}

#endif // !__cplusplus

#endif // HAL_H