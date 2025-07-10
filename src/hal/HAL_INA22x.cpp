#include "HAL.h"

INA226 ina(Wire);

float ShuntVoltage_mV;
float BusVoltage;
float ShuntCurrent;

void HAL::INA22x_Init(){
    ina.begin(0x40);
    if (!ina.begin(0x40))
    {
        while (!ina.begin())
        {
            delay(50);
            Wire.end(); //关闭I2C
            delay(50);
            Wire.begin(36,35); //开发板引脚
            ina.begin(0x40);
            break;
        }
        
    }
    ina.calibrate(Sampling_ohm,10);
    ina.configure(INA226_AVERAGES_64,INA226_BUS_CONV_TIME_332US,INA226_SHUNT_CONV_TIME_332US,INA226_MODE_SHUNT_BUS_CONT);//~20sps 48,896μs
}

void HAL::INA22x_Run(){
    NowTime = millis();
    ShuntVoltage_mV = ina.readShuntVoltage();
    BusVoltage = ina.readBusVoltage();
    ShuntCurrent = ina.readShuntCurrent();

    LoadVoltage = fabs(BusVoltage + (ShuntVoltage_mV/1000));
    LoadCurrent = fabs(ShuntCurrent);
    LoadPower = fabs(ina.readBusPower());
    
    CurrentDirection = (ShuntCurrent < 0) ? 1 : 0;

    mAh = mAh + (LoadVoltage * (NowTime - LastTime))/3600;
    mWh = mWh + (LoadPower * ((NowTime - LastTime)/3600));
    Ah = mWh/1000;
    Wh = mWh/1000;

    LastTime = NowTime;
}