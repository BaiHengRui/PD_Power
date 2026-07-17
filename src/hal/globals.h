#pragma once
// ============================================================
// 全局变量声明 (定义于 globals.cpp)
// ============================================================
#include <stdint.h>
#include <TFT_eSPI.h>
#include "hal.h"

// --- TFT 实例 (全局共享) ---
extern TFT_eSPI tft;

// --- INA 数据 ---
extern HAL::INA226_Data INA;

// --- PD 数据 ---
extern HAL::PD_Data PD;

// --- 系统 ---
extern uint64_t SNID;
extern uint64_t nowTime_us, lastTime_us;
extern uint32_t startTime;

// --- 应用状态 ---
extern uint8_t  nowApp, maxApp;

// --- 显示 ---
extern uint8_t  defaultBrightness;
extern uint8_t  defaultRotation;
extern uint8_t  currentRotation;
extern int8_t   pendingRotation;
extern int      LCD_InvertDisplay;
extern int      lcdBrightness;
extern int      LCD_Rotation;
extern float    currentFPS;

// --- ADC ---
extern float v_DN, v_DP;
extern float v_CC1, v_CC2;
extern float NTC_Temperature;

// --- Toast ---
#define TOAST_DURATION_MS 1500
extern char     toastMsg[40];
extern uint32_t toastStart;

// --- Legacy compat ---
extern int      OTA_Progress;
extern String   btnStatus;
extern char     logLine[256];
extern bool     PD_Enable;

// --- Graph (ring buffer, sticky auto-scale) ---
#define GRAPH_WIDTH 240  // 匹配 240px 宽显示屏

extern float    voltageBuffer[GRAPH_WIDTH];
extern float    currentBuffer[GRAPH_WIDTH];
extern int      graphIndex;
extern bool     graphDataInitialized;
extern bool     graphRangeInitialized;
extern bool     graphPaused;
extern float    vDisplayMin, vDisplayMax, vHistoryMax;
extern float    iDisplayMin, iDisplayMax, iHistoryMax;
extern float    frozenVoltage, frozenCurrent;

// --- 蜂鸣器 ---
extern HAL::Buzzer_State Buzzer;
