// ============================================================
// 全局变量定义
// ============================================================
#include "hal.h"
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// --- INA 数据 ---
HAL::INA226_Data INA = {0};

// --- PD 数据 ---
HAL::PD_Data PD = {0};

// --- 系统 ---
uint64_t SNID         = 0;
uint64_t nowTime_us   = 0;
uint64_t lastTime_us  = 0;
uint32_t startTime    = 0;

// --- 应用状态 ---
uint8_t  nowApp       = AppState::MAIN;
uint8_t  maxApp       = AppState::OTA_FAIL;

// --- 显示 ---
uint8_t  defaultBrightness = 50;
uint8_t  defaultRotation   = 0;
uint8_t  currentRotation   = 0;
int8_t   pendingRotation   = -1;
int      LCD_InvertDisplay = 0;  // 由 TFT_eSPI 配置管理反转
int      lcdBrightness     = 50;
int      LCD_Rotation      = 0;
float    currentFPS        = 0;

// --- ADC ---
float v_DN = 0, v_DP = 0;
float v_CC1 = 0, v_CC2 = 0;
float NTC_Temperature = 0;

// --- Toast ---
char     toastMsg[40]  = {0};
uint32_t toastStart    = 0;

// --- Buzzer ---
HAL::Buzzer_State Buzzer = {false};

// --- Legacy compat ---
int    OTA_Progress = 0;
String btnStatus    = "";
char   logLine[256] = {0};
bool   PD_Enable    = false;

// --- Graph (ring buffer, sticky auto-scale) ---
float    voltageBuffer[GRAPH_WIDTH] = {0};
float    currentBuffer[GRAPH_WIDTH] = {0};
int      graphIndex          = 0;
bool     graphDataInitialized = false;
bool     graphRangeInitialized = false;
bool     graphPaused          = false;
float    vDisplayMin = 0.0f, vDisplayMax = 5.0f, vHistoryMax = 0.0f;
float    iDisplayMin = 0.0f, iDisplayMax = 2.0f, iHistoryMax = 0.0f;
float    frozenVoltage = 0.0f, frozenCurrent = 0.0f;
