#include <Arduino.h>
#include "hal/HAL.h"
#include "hal/Config.h"

void setup() {
  // put your setup code here, to run once:
  HAL::Sys_Init();
}

void loop() {
  // put your main code here, to run repeatedly:
  HAL::Sys_Run();
}
