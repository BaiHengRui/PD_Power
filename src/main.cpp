// ============================================================
// PD_Power main (ESP32C3_USB_METER architecture)
// ============================================================
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "hal/hal.h"

// --- 任务句柄 ---
TaskHandle_t xTaskSensor  = NULL;
TaskHandle_t xTaskDisplay = NULL;
TaskHandle_t xTaskPD      = NULL;
TaskHandle_t xTaskButton  = NULL;
TaskHandle_t xTaskGraph   = NULL;
TaskHandle_t xTaskWiFi    = NULL;

// --- 任务函数 ---
void Task_Sensor(void* pv);
void Task_Display(void* pv);
void Task_PD(void* pv);
void Task_Button(void* pv);
void Task_Graph(void* pv);
void Task_WiFi(void* pv);

void setup()
{
    // 系统初始化
    HAL::Sys_Init();
    HAL::GPIO_Init();
    HAL::Buzzer_Init();
    HAL::Button_Init();
    HAL::INA226_Init();
    HAL::LCD_Init();
    HAL::PD_Init();

    // 创建 FreeRTOS 任务
    xTaskCreatePinnedToCore(Task_Sensor,  "Sensor",  3072, NULL, 3, &xTaskSensor,  0);
    xTaskCreatePinnedToCore(Task_PD,      "PD",      4096, NULL, 2, &xTaskPD,      0);
    xTaskCreatePinnedToCore(Task_Button,  "Button",  3072, NULL, 4, &xTaskButton,  0);
    xTaskCreatePinnedToCore(Task_Graph,   "Graph",   2048, NULL, 2, &xTaskGraph,   0);
    xTaskCreatePinnedToCore(Task_WiFi,    "WiFi",    8192, NULL, 2, &xTaskWiFi,    1);  // Core 1: OTA/WebServer
    xTaskCreatePinnedToCore(Task_Display, "Display", 8192, NULL, 1, &xTaskDisplay, 0);  // Core 1 避免阻塞 IDLE0

    HAL::Buzzer_Beep(100);
    HAL::LOG_INFO("All tasks started");
    vTaskDelete(NULL);
}

void loop() { /* FreeRTOS tasks handle everything */ }

// ============================================================
// Sensor Task: INA226 + GPIO 读取 (200Hz)
// ============================================================
void Task_Sensor(void* pv)
{
    TickType_t xLastWake = xTaskGetTickCount();
    for (;;) {
        HAL::INA226_GetData(&INA);
        HAL::GPIO_Read();
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(5));
    }
}

// ============================================================
// PD Task: PD Sniffer 监听 (100Hz)
// ============================================================
void Task_PD(void* pv)
{
    TickType_t xLastWake = xTaskGetTickCount();
    for (;;) {
        HAL::PD_Run();
        HAL::PD_GetData(&PD);
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(10));
    }
}

// ============================================================
// Button Task: 按键扫描 (100Hz, 高优先级)
// ============================================================
void Task_Button(void* pv)
{
    TickType_t xLastWake = xTaskGetTickCount();
    for (;;) {
        HAL::Button_Loop();
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(10));
    }
}

// ============================================================
// Graph Task: 曲线 ring buffer 更新 (50Hz)
// ============================================================
void Task_Graph(void* pv)
{
    TickType_t xLastWake = xTaskGetTickCount();
    for (;;) {
        HAL::Update_Graph_Data();
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(20));
    }
}

// ============================================================
// WiFi/OTA Task: WiFiManager + WebServer (100Hz)
// ============================================================
void Task_WiFi(void* pv)
{
    TickType_t xLastWake = xTaskGetTickCount();
    for (;;) {
        HAL::WiFi_Loop();
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(10));
    }
}

// ============================================================
// Display Task: UI 渲染 (25 FPS)
// ============================================================
void Task_Display(void* pv)
{
    TickType_t xLastWake = xTaskGetTickCount();
    for (;;) {
        HAL::UI_Run();
        esp_task_wdt_reset();  // 喂狗，防止 SPI 操作超时触发 WDT
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(40));
    }
}
