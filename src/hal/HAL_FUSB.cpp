#include "HAL.h"
#include "Config.h"

PD_UFP_Log_c PD_UFP(PD_LOG_LEVEL_VERBOSE);

void HAL::PD_Init() {
    PD_UFP.init_PPS(FUSB302_INT_PIN,PPS_V(PD_POWER_OPTION_MAX_VOLTAGE), PPS_A(PD_POWER_OPTION_MAX_CURRENT), PD_POWER_OPTION_MAX_POWER);
    Serial.println("FUSB302 PD Sink Init!");
}

void HAL::PD_Run() {
    PD_UFP.run();
    static long PDtimeMillis = millis();
    if (millis() - PDtimeMillis >= 200) { // 200ms
        PDtimeMillis = millis();
        char buf[128];
        PD_UFP.status_log_readline(buf, sizeof(buf) - 1);
        Serial.printf("%s", buf);
    }
    
    PD_Voltage = PD_UFP.get_voltage();
    PD_Current = PD_UFP.get_current();
    
    status_power_t status = PD_UFP.get_ps_status();
    if (status == STATUS_POWER_TYP) {
        PD_Ready = 1; // 电源就绪 (STATUS_POWER_TYP)
        PD_Option = 0; // 当前为固定电压模式 (TYP)
    } else if (status == STATUS_POWER_PPS) {
        PD_Ready = 1; // 电源就绪 (STATUS_POWER_PPS)
        PD_Option = 1; // 当前为可编程电源模式 (PPS)
    } else {
        PD_Ready = 0; // 电源未就绪 (STATUS_POWER_NA)
        PD_Voltage = 0;
        PD_Current = 0;
    }
    PD_Src_Cap_Count = PD_UFP.get_src_cap_count(); // 获取源能力计数
    PD_Position = PD_UFP.get_selected_position(); // 获取PD位置
    ccbus_used = PD_UFP.get_cc_pin(); // 获取CC线状态，0/NULL 1/CC1 2/CC2
}