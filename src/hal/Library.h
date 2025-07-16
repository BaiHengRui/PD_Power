#ifndef LIBRARY_H
#define LIBRARY_H

#include <Arduino.h>
#include <esp32-hal-cpu.h>
#include <esp32-hal-gpio.h>
#include <esp32-hal-adc.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_task_wdt.h>

#include <EEPROM.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ESPmDNS.h>

#include <TFT_eSPI.h>
#include <INA226.h>
#include <Button2.h>
#include <PD_UFP.h>
#include <WiFiManager.h>
#include <ESP32AnalogRead.h>

#endif // LIBRARY_H