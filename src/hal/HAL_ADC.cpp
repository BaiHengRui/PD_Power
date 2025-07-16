#include "HAL.h"
#include "Config.h"


void HAL::ADC_Init(){
  
}

void HAL::ADC_Sampling() {
    adcBuffer[bufferIndex] = analogRead(VBUS_ADC);
    bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;  // 循环索引
    // vTaskDelay(pdMS_TO_TICKS(1));
}