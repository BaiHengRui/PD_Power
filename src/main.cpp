#include <Arduino.h>
#include "hal/HAL.h"
#include "hal/Config.h"

TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t updateTaskHandle = NULL;
TaskHandle_t waveformTaskHandle = NULL;

void sensorTask(void *pvParameters);
void displayTask(void *pvParameters);
void updateTask(void *pvParameters);
void waveformTask(void *pvParameters);

void sensorTask(void *pvParameters){
  for (;;)
  {
    HAL::INA22x_Run();
    HAL::GPIO_Run();
    HAL::PD_Run();
    vTaskDelay(pdMS_TO_TICKS(10)); //1ms delay 1k sps
  }
  
}

void displayTask(void *pvParameters){
  for (;;)
  {
        frameCount++;
        unsigned long currentTime = millis();
        
        // 每 1000ms (1秒) 计算一次帧率
        if (currentTime - lastFPSTime >= 1000) {
            currentFPS = frameCount * 1000.0 / (currentTime - lastFPSTime);
            frameCount = 0;
            lastFPSTime = currentTime;
        }

    HAL::Sys_Run();

    vTaskDelay(pdMS_TO_TICKS(20)); //20ms delay 50 fps
  }
  
}

void updateTask(void *pvParameters){
  for (;;)
  {
    HAL::WebUptadeRun(); //OTA Task
    esp_task_wdt_reset(); // Reset watchdog
    vTaskDelay(pdMS_TO_TICKS(1)); //1ms delay 
  }
  
}

void waveformTask(void *pvParameters){
  for (;;)
  {
    HAL::ADC_Sampling();
    esp_task_wdt_reset(); // Reset watchdog
    vTaskDelay(pdMS_TO_TICKS(1)); //1ms delay
  }
  
}
void setup() {
  // put your setup code here, to run once:
  setCpuFrequencyMhz(240); //Full
  esp_task_wdt_init(10,false); //watch dog 10s time out
  HAL::Sys_Init();
  HAL::LCD_Light_Updat(1,0);
  HAL::ADC_Init();
  Now_App = 3;
  xTaskCreatePinnedToCore(sensorTask,"Sensor",4096,NULL,1,&sensorTaskHandle,1);
  xTaskCreatePinnedToCore(displayTask,"Display",8192,NULL,2,&displayTaskHandle,1);
  xTaskCreatePinnedToCore(updateTask,"Update",8192,NULL,3,&updateTaskHandle,1);
  xTaskCreatePinnedToCore(waveformTask,"Waveform",4096,NULL,2,&waveformTaskHandle,0);
}

void loop() {
  // put your main code here, to run repeatedly:

}
