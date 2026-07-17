// ============================================================
// PD Sniffer module (PDLib v2.0)
// ============================================================
#include "hal.h"
#include <PD_Sniffer.h>

static PD_Sniffer sniffer;
static bool pd_ok = false;

bool HAL::PD_Init()
{
    if (sniffer.begin(FUSB302_INT_PIN)) {
        HAL::LOG_INFO("PD Sniffer ready (passive monitor)");
        pd_ok = true;
        return true;
    }
    HAL::LOG_INFO("PD Sniffer init failed!");
    return false;
}

void HAL::PD_Run()
{
    if (!pd_ok) return;
    sniffer.update();
}

void HAL::PD_GetData(PD_Data* data)
{
    if (!data || !pd_ok) return;

    data->voltage       = sniffer.getVoltage();
    data->current       = sniffer.getCurrent();
    data->power         = sniffer.getPower();
    data->connected     = sniffer.isConnected();
    data->is_pps        = sniffer.isPPS();

    if (sniffer.isConnected()) {
        PD_PowerStatus st = sniffer.getPowerStatus();
        data->ready = (st == PD_POWER_FIXED || st == PD_POWER_PPS);
    } else {
        data->ready = false;
    }

    data->cc_pin        = sniffer.getCCPin();
    data->selected_pos  = sniffer.getSelectedPosition();
    data->pdo_count     = sniffer.getSourcePDOCount();
    data->packet_count  = sniffer.getPacketCount();

    strncpy(data->last_msg, sniffer.getLastMessageName(), sizeof(data->last_msg) - 1);
    data->last_msg[sizeof(data->last_msg) - 1] = '\0';  // 确保 null 终止

    // 更新 PD 监听日志行
    if (!data->connected) {
        snprintf(logLine, sizeof(logLine), "Disconnected");
    } else if (data->is_pps) {
        snprintf(logLine, sizeof(logLine), "PPS %.1fV %.1fA %s SRC:%d POS:%d CC%d",
                 data->voltage, data->current, data->ready ? "RDY" : "---",
                 (int)data->pdo_count, (int)data->selected_pos, (int)data->cc_pin);
    } else {
        snprintf(logLine, sizeof(logLine), "FIX %.1fV %.1fA %s %s SRC:%d POS:%d CC%d",
                 data->voltage, data->current, data->ready ? "RDY" : "---",
                 data->last_msg, (int)data->pdo_count, (int)data->selected_pos, (int)data->cc_pin);
    }
}
