// ============================================================
// HAL system functions
// ============================================================
#include "hal.h"

void HAL::Sys_Init()
{
    Serial.begin(115200);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    pinMode(LCD_BL_PIN, OUTPUT);
    digitalWrite(LCD_BL_PIN, HIGH);

    SNID = ESP.getEfuseMac();
    startTime = millis();

    HAL::NVS_Init();
    HAL::NVS_Load();

    HAL::LOG_INFO("PD_Power " SOFTWARE_VERSION " booting...");
    HAL::LOG_INFO("SN: " + String((uint32_t)(SNID >> 32), HEX) + String((uint32_t)SNID, HEX));
}

void HAL::LOG_INFO(const String& msg)
{
    Serial.print("[");
    Serial.print(millis());
    Serial.print("] ");
    Serial.println(msg);
}

String HAL::Get_System_RunTime(uint32_t us)
{
    uint64_t totalSec = us / 1000000UL;
    uint32_t h = totalSec / 3600;
    uint32_t m = (totalSec % 3600) / 60;
    uint32_t s = totalSec % 60;
    char buf[12];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, m, s);
    return String(buf);
}

String HAL::Get_System_Status()
{
    if (PD.ready) {
        return PD.is_pps ? "PPS" : "FIX";
    }
    return "IDLE";
}

float HAL::Get_CPU_Temperature()
{
    return temperatureRead();
}

void HAL::ShowToast(const char* msg)
{
    strncpy(toastMsg, msg, sizeof(toastMsg) - 1);
    toastMsg[sizeof(toastMsg) - 1] = '\0';
    toastStart = millis();
}

bool HAL::IsToastActive()
{
    return (millis() - toastStart < TOAST_DURATION_MS) && (toastMsg[0] != '\0');
}

// ============================================================
// Graph: ring buffer + sticky auto-scale (ESP32C3_USB_METER pattern)
// ============================================================
void HAL::Update_Graph_Data()
{
    static bool wasPaused = false;

    float newVoltage = INA.voltage;
    float newCurrent = INA.current;

    // 暂停上升沿：冻结当前值
    if (!wasPaused && graphPaused) {
        frozenVoltage = newVoltage;
        frozenCurrent = newCurrent;
    }
    wasPaused = graphPaused;

    // 只在非暂停时采样
    if (!graphPaused) {
        voltageBuffer[graphIndex] = newVoltage;
        currentBuffer[graphIndex] = newCurrent;
        graphIndex = (graphIndex + 1) % GRAPH_WIDTH;
        graphDataInitialized = true;

        // 更新历史最大值
        if (newVoltage > vHistoryMax) vHistoryMax = newVoltage;
        if (newCurrent > iHistoryMax) iHistoryMax = newCurrent;

        // Sticky auto-scale: 只扩大, 不缩小
        const float marginFactor = 0.05f;

        if (!graphRangeInitialized) {
            vDisplayMin = 0.0f;
            vDisplayMax = fmaxf(0.1f, newVoltage * (1.0f + marginFactor));
            iDisplayMin = 0.0f;
            iDisplayMax = fmaxf(0.1f, newCurrent * (1.0f + marginFactor));
            graphRangeInitialized = true;
        } else {
            if (newVoltage > vDisplayMax) {
                vDisplayMax = newVoltage * (1.0f + marginFactor);
            }
            if (newCurrent > iDisplayMax) {
                iDisplayMax = newCurrent * (1.0f + marginFactor);
            }
            vDisplayMin = 0.0f;
            iDisplayMin = 0.0f;
        }

        if (vDisplayMax <= vDisplayMin) vDisplayMax = vDisplayMin + 0.1f;
        if (iDisplayMax <= iDisplayMin) iDisplayMax = iDisplayMin + 0.1f;
    }
}
