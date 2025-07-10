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

}


#endif // !__cplusplus

#endif // HAL_H