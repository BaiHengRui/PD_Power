// ============================================================
// NVS persistent storage (replaces EEPROM)
// ============================================================
#include "hal.h"

static Preferences prefs;

void HAL::NVS_Init() {
    prefs.begin("pd_power", false);
}

void HAL::NVS_Load() {
    defaultBrightness = NVS_ReadUChar("light",    50);
    defaultRotation   = NVS_ReadUChar("rotation", 0);
}

void HAL::NVS_Save() {
    NVS_WriteUChar("light",    defaultBrightness);
    NVS_WriteUChar("rotation", defaultRotation);
}

uint8_t HAL::NVS_ReadUChar(const char* key, uint8_t def) {
    return prefs.getUChar(key, def);
}
void HAL::NVS_WriteUChar(const char* key, uint8_t val) {
    prefs.putUChar(key, val);
}
uint32_t HAL::NVS_ReadUInt(const char* key, uint32_t def) {
    return prefs.getUInt(key, def);
}
void HAL::NVS_WriteUInt(const char* key, uint32_t val) {
    prefs.putUInt(key, val);
}
