// ============================================================
// INA226 measurement module
// ============================================================
#include "hal.h"
#include <INA226.h>

static INA226 ina(Wire);

bool HAL::INA226_Init()
{
    if (!ina.begin(0x40)) {
        HAL::LOG_INFO("INA226 not found!");
        return false;
    }
    // 5mΩ 采样电阻
    ina.calibrate(0.005f, 8);
    ina.configure(INA226_AVERAGES_16, INA226_CONV_TIME_140US,
                  INA226_CONV_TIME_140US, INA226_MODE_SHUNT_BUS_CONT);
    HAL::LOG_INFO("INA226 ready (5mΩ, 8A max)");
    return true;
}

void HAL::INA226_GetData(INA226_Data* data)
{
    if (!data) return;

    nowTime_us = esp_timer_get_time();

    data->bus_V    = ina.readBusVoltage();
    data->shunt_mV = ina.readShuntVoltage() / 1000.0f;
    data->voltage  = fabsf(data->bus_V + data->shunt_mV);
    data->current  = fabsf(ina.readCurrent());
    data->power    = fabsf(ina.readPower());
    data->current_direction = (ina.readCurrent() < 0);

    float deltaSec = (nowTime_us - lastTime_us) / 1000000.0f;
    if (lastTime_us == 0) deltaSec = 0;

    data->charge_mAh += (data->current * 1000.0f) * (deltaSec / 3600.0f);
    data->energy_mWh += (data->power   * 1000.0f) * (deltaSec / 3600.0f);
    data->charge_Ah   = data->charge_mAh / 1000.0f;
    data->energy_Wh   = data->energy_mWh / 1000.0f;
    data->temperature = HAL::Get_CPU_Temperature();  // INA226 无温度传感器

    lastTime_us = nowTime_us;
}
