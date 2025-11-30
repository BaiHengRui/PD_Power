#include "hal.h"
#include "Config.h"

PD_UFP_Log_c PD_UFP(PD_LOG_LEVEL_VERBOSE);

void HAL::PD_Init() {
    // PD_UFP.init_PPS(FUSB302_INT_PIN,PPS_V(PD_POWER_OPTION_MAX_VOLTAGE), PPS_A(PD_POWER_OPTION_MAX_CURRENT), PD_POWER_OPTION_MAX_POWER);
    // PD_UFP.init_PPS(FUSB302_INT_PIN,PPS_V(20),PPS_A(5));
    PD_UFP.init_Bridge(FUSB302_INT_PIN);
    PD_UFP.enable_vbus_sense(0); // 关闭 VBUS 检测
    Serial.println("FUSB302 PD Sink Init!");
}

void HAL::PD_Run() {
    PD_UFP.run_Bridge();
    static long PDtimeMillis = millis(); 
    if (millis() - PDtimeMillis >= 200) { // 200ms
        PDtimeMillis = millis();
        PD_UFP.status_bridge_log_readline(pdbuf, sizeof(pdbuf) - 1);
        // Serial.printf(buf);
    }
    
    PD_Voltage = PD_UFP.get_bridge_voltage();
    PD_Current = PD_UFP.get_bridge_current();
    
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
    PD_Src_Cap_Count = PD_UFP.get_bridge_src_cap_count(); // 获取源能力计数
    PD_Position = PD_UFP.get_bridge_selected_position(); // 获取PD位置
    ccbus_used = PD_UFP.get_bridge_cc_pin(); // 获取CC线状态，0/NULL 1/CC1 2/CC2
}

void HAL::QC_Init() {
    // Quick Charge initialization code can be added here
    Serial.println("Quick Charge Init!");
    digitalWrite(DP_PIN, 75);
    delay(1250);
    digitalWrite(DP_PIN, 0);
}

void HAL::QC_Run() {
    // Quick Charge run code can be added here
    // For now, just a placeholder
    Serial.println("Quick Charge Run!");
}
//开学了，项目搁置一会，后续会把菜单完善