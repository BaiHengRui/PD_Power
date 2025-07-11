#ifndef GLOBAL_VARIABLES_H
#define GLOBAL_VARIABLES_H

#include <Arduino.h>

/*LCD Init*/
extern int LCD_InvertDisplay;
extern int LCD_Light;
extern int LCD_Rotation;

/*INA*/
extern float Sampling_ohm;
extern float LoadVoltage;
extern float LoadCurrent;
extern float LoadPower;
extern bool CurrentDirection;
extern float mAh, mWh, Ah, Wh;
extern float Capacity, Energy;
extern float Record_Power;
extern float MaxVoltage, MaxCurrent, MaxPower;
extern float MinVoltage, MinCurrent, MinPower;

/*ADC*/
extern float v_DN, v_DP;
extern float v_CC1, v_CC2;
extern float NTCv, NTCm, NTC_Temperature;

/*PD*/
extern char buf[128];
extern bool PD_Option; // default 0 FIX/1 PPS
extern bool PD_Ready; // default 0 not ready/1 ready
extern float PD_Voltage, PD_Current;
extern int PD_Src_Cap_Count; // PD Source Capability Count
extern int PD_Position; // PD Position
extern int ccbus_used; // CC线状态 0/NULL 1/CC

/*System*/
extern uint64_t SNID;
extern int32_t NowTime, LastTime;
extern float OTA_Progress;
extern int Now_App;
extern int All_Apps;
extern int LastApp;
extern int Menu_Key;
extern float CPU_Temperature;
extern uint32_t Sketch_Size;
extern uint32_t Free_Flash_Size;

/*EEPROM*/
extern int EEPROM_Light_addr;
extern int EEPROM_Rotation_addr;
extern int EEPROM_App_addr;
extern int EEPROM_WiFi_addr;

#endif // GLOBAL_VARIABLES_H