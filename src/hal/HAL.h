#pragma once

// ============================================================
// PD_Power — HAL 主头文件
// ============================================================

#include <Arduino.h>
#include <stdint.h>
#include <esp32-hal-cpu.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <Wire.h>

// ============================================================
// 一、硬件引脚定义
// ============================================================
#define I2C_SDA_PIN     36
#define I2C_SCL_PIN     35
#define FUSB302_INT_PIN 38
#define LCD_BL_PIN      9
#define BUZZER_PIN      33
#define BUZZER_FREQ     4000
#define VBUS_ADC        7
#define DN_PIN          3
#define DP_PIN          4
#define CC1_PIN         1
#define CC2_PIN         2
#define NTC_PIN         17
#define SW1             0
#define SW2             21
#define SW3             8
#define SW4             18

// ============================================================
// 二、版本号
// ============================================================
#define SOFTWARE_VERSION "v2.0.0"
#define HARDWARE_VERSION "v1.0.0"

// ============================================================
// 三、HAL 数据类型
// ============================================================
namespace HAL
{
    /// INA226 测量数据
    typedef struct
    {
        float voltage;              // 总线电压 (V)
        float current;              // 电流 (A)
        float power;                // 功率 (W)
        float energy_mWh;           // 累计能量 (mWh)
        float charge_mAh;           // 累计电量 (mAh)
        float energy_Wh;            // 累计能量 (Wh)
        float charge_Ah;            // 累计电量 (Ah)
        float temperature;          // 温度 (℃)
        bool  current_direction;    // true=充电, false=放电
        float shunt_mV;             // 分流电压 (mV)
        float bus_V;                // 总线电压原始值
    } INA226_Data;

    /// PD 监听数据
    typedef struct
    {
        float    voltage;           // 协商电压 (V)
        float    current;           // 协商电流 (A)
        float    power;             // 协商功率 (W)
        bool     connected;         // CC 已连接
        bool     ready;             // 供电就绪
        bool     is_pps;            // PPS 模式
        uint8_t  cc_pin;            // CC 引脚 (0/1/2)
        uint8_t  selected_pos;      // 当前 PDO 位置
        uint8_t  pdo_count;         // Source PDO 数量
        uint32_t packet_count;      // PD 包计数
        char     last_msg[20];      // 最近消息类型
    } PD_Data;

    /// 蜂鸣器状态
    typedef struct
    {
        bool mute;
    } Buzzer_State;
}

// ============================================================
// 四、HAL 功能接口
// ============================================================
namespace HAL
{
    /* --- System --- */
    void   Sys_Init();
    void   LOG_INFO(const String& msg);
    String Get_System_RunTime(uint32_t us);
    String Get_System_Status();
    float  Get_CPU_Temperature();

    /* --- NVS 持久化存储 --- */
    void     NVS_Init();
    void     NVS_Load();
    void     NVS_Save();
    uint8_t  NVS_ReadUChar(const char* key, uint8_t def);
    void     NVS_WriteUChar(const char* key, uint8_t val);
    uint32_t NVS_ReadUInt(const char* key, uint32_t def);
    void     NVS_WriteUInt(const char* key, uint32_t val);

    /* --- INA226 --- */
    bool INA226_Init();
    void INA226_GetData(INA226_Data* data);

    /* --- PD Sniffer --- */
    bool PD_Init();
    void PD_Run();
    void PD_GetData(PD_Data* data);

    /* --- LCD / TFT --- */
    void LCD_Init();
    void LCD_SetBrightness(uint8_t brightness);
    void LCD_SetRotation(uint8_t rotation);
    void LCD_Refresh(uint32_t bgColor = 0x0000);
    void LCD_Refresh_Screen(uint32_t bgcolor);
    void LCD_Backlight(bool on);
    void LCD_Light_Updat(int light, bool saved);
    void LCD_Rotation_Update(int rotation, bool saved);

    /* --- Button --- */
    void Button_Init();
    void Button_Loop();

    /* --- Buzzer --- */
    void Buzzer_Init();
    void Buzzer_Beep(uint16_t ms = 100);
    void Buzzer_SetMute(bool mute);

    /* --- GPIO / ADC --- */
    void GPIO_Init();
    void GPIO_Read();

    /* --- Web / OTA --- */
    void WiFi_Init();
    void WiFi_Loop();
    void WiFi_Connect();
    void OTA_Start();
    void OTA_Check();

    /* --- UI --- */
    void UI_Run();
    void UI_Main();
    void UI_VBUS_Curve();
    void UI_Page1();
    void UI_Menu();
    void UI_Setting();
    void UI_SystemInfo();
    void UI_PowerDelivery();
    void UI_QuickCharge();
    void UI_LOG();
    void UI_WiFi_Connect();
    void UI_WiFi_Connect_Fail();
    void UI_OTA_Update();
    void UI_OTA_Finish();
    void UI_OTA_Fail();

    /* --- Graph --- */
    void Update_Graph_Data();

    /* --- Toast --- */
    void ShowToast(const char* msg);
    bool IsToastActive();
} // namespace HAL

// ============================================================
// 五、应用状态
// ============================================================
namespace AppState
{
    constexpr uint8_t MAIN           = 0;
    constexpr uint8_t VBUS_CURVE     = 1;
    constexpr uint8_t PAGE1          = 2;
    constexpr uint8_t MENU           = 3;
    constexpr uint8_t SETTING        = 4;
    constexpr uint8_t SYSTEM_INFO    = 5;
    constexpr uint8_t PD_INFO        = 6;
    constexpr uint8_t QC_INFO        = 7;
    constexpr uint8_t LOG            = 8;
    constexpr uint8_t WIFI_CONNECT   = 9;
    constexpr uint8_t WIFI_FAIL      = 10;
    constexpr uint8_t OTA_UPDATE     = 11;
    constexpr uint8_t OTA_FINISH     = 12;
    constexpr uint8_t OTA_FAIL       = 13;
}

// ============================================================
// 六、全局变量声明
// ============================================================
#include "globals.h"
