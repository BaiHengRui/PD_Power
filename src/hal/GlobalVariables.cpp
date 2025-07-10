#include "GlobalVariables.h"

int LCD_InvertDisplay = 1; // 0: Normal, 1: Inverted
int LCD_Light = 50; // 0-100% brightness
int LCD_Rotation = 0; // 0: 0 degrees, 1: 90 degrees, 2: 180 degrees, 3: 270 degrees

float Sampling_ohm = 0.005f; // 5mOhm
float LoadVoltage = 0.0f;
float LoadCurrent = 0.0f;
float LoadPower = 0.0f;
bool CurrentDirection = 0; // 0: Discharging, 1: Charging
float mAh = 0.0f, mWh = 0.0f, Ah = 0.0f, Wh = 0.0f;
float Capacity = 0.0f, Energy = 0.0f;
float Record_Power = 0.0f;
float MaxVoltage = 5.0f, MaxCurrent = 0.0f, MaxPower = 0.0f;
float MinVoltage = 5.0f, MinCurrent = 0.0f, MinPower = 0.0f;

float v_DN = 0.0f, v_DP = 0.0f;
float v_CC1 = 0.0f, v_CC2 = 0.0f;
float NTCv = 0.0f, NTCm = 0.0f, NTC_Temperature = 0.0f;

bool PD_Option = 0; // default 0 FIX/1 PPS
bool PD_Ready = 0; // 0 not ready/1 ready
float PD_Voltage = 0.0f, PD_Current = 0.0f;
int PD_Src_Cap_Count = 0; // PD Source Capability Count
int PD_Position = 0; // PD Position
int ccbus_used = 0; // CC线状态 0/NULL 1/CC1 2/CC2

uint64_t SNID = 0;
int32_t NowTime = 0, LastTime = 0;
int OTA_Progress = 0;
bool OTA_Update_Status = 0;
int Now_App = 1; // Current app index
int All_Apps = 13; // Total number of apps
int LastApp = 0; // Last app index
int Menu_Key = 0; // Menu key number
float CPU_Temperature = 0.0f;
uint32_t Sketch_Size = 0; // Size of the current sketch
uint32_t Free_Flash_Size = 0; // Free flash size

int EEPROM_Light_addr = 1; // EEPROM address for LCD light
int EEPROM_Rotation_addr = 2; // EEPROM address for LCD rotation
int EEPROM_App_addr = 3; // EEPROM address for current app
int EEPROM_WiFi_addr = 23; // EEPROM address for WiFi configuration

