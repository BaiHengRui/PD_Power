#include "HAL.h"

void HAL::Sys_Init() {
    Serial.begin(115200);
    Wire.begin();
    EEPROM.begin(512);
    
    GPIO_Init();
    INA22x_Init();
    PD_Init();
    Buzzer_Init();
    LCD_Init();

}

void HAL::Sys_Run() {
    INA22x_Run();
    GPIO_Run();
    
}