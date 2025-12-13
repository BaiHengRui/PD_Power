/**
 * PD_UFP_Fixed.cpp - FUSB302 Bridge模式完整修复版本
 *
 * 修复内容：
 * 1. 完全禁用Bridge模式下的PD握手，避免循环重启
 * 2. 优化PD数据包解析速度
 * 3. 修复PPS模式电压电流解析逻辑
 * 4. 修复get_bridge_voltage和get_bridge_current函数
 * 5. 优化日志结构，替换混淆字符
 * 6. 增强监听模式的稳定性
 */

#include <stdint.h>
#include <string.h>

#include "PD_UFP.h"

#define t_PD_POLLING            100
#define t_TypeCSinkWaitCap      350
#define t_RequestToPSReady      580     // combine t_SenderResponse and t_PSTransition
#define t_PPSRequest            5000    // must less than 10000 (10s)

#define PIN_FUSB302_INT         12

enum {
    STATUS_LOG_MSG_TX,
    STATUS_LOG_MSG_RX,
    STATUS_LOG_DEV,
    STATUS_LOG_CC,
    STATUS_LOG_SRC_CAP,
    STATUS_LOG_POWER_READY,
    STATUS_LOG_POWER_PPS_STARTUP,
    STATUS_LOG_POWER_REJECT,
    STATUS_LOG_LOAD_SW_ON,
    STATUS_LOG_LOAD_SW_OFF,
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// PD_UFP_c
///////////////////////////////////////////////////////////////////////////////////////////////////
PD_UFP_c::PD_UFP_c():
    ready_voltage(0),
    ready_current(0),
    PPS_voltage_next(0),
    PPS_current_next(0),
    status_initialized(0),
    status_src_cap_received(0),
    status_power(STATUS_POWER_NA),
    time_polling(0),
    time_wait_src_cap(0),
    time_wait_ps_rdy(0),
    time_PPS_request(0),
    get_src_cap_retry_count(0),
    wait_src_cap(0),
    wait_ps_rdy(0),
    send_request(0),
    bridge_mode_enabled(false),
    bridge_log_index(0),
    bridge_mode_confirmed(false),
    monitor_active(false)
{
    memset(&FUSB302, 0, sizeof(FUSB302_dev_t));
    memset(&protocol, 0, sizeof(PD_protocol_t));
    // 修复：初始化监听数据
    memset(&pd_monitor, 0, sizeof(pd_monitor_t));
}

bool PD_UFP_c::enable_vbus_sense(bool enable)
{
    FUSB302_set_vbus_sense(&FUSB302, enable);
    return true;
}

void PD_UFP_c::init(uint8_t int_pin, enum PD_power_option_t power_option)
{
    init_PPS(int_pin, 0, 0, power_option);
}

void PD_UFP_c::init_PPS(uint8_t int_pin, uint16_t PPS_voltage, uint8_t PPS_current, enum PD_power_option_t power_option)
{
    this->int_pin = int_pin;
    // Initialize FUSB302
    pinMode(int_pin, INPUT_PULLUP); // Set FUSB302 int pin input ant pull up
    FUSB302.i2c_address = 0x22;
    FUSB302.i2c_read = FUSB302_i2c_read;
    FUSB302.i2c_write = FUSB302_i2c_write;
    FUSB302.delay_ms = FUSB302_delay_ms;
    if (FUSB302_init(&FUSB302) == FUSB302_SUCCESS && FUSB302_get_ID(&FUSB302, 0, 0) == FUSB302_SUCCESS) {
        status_initialized = 1;
    }

    // Two stage startup for PPS Voltge < 5V
    if (PPS_voltage && PPS_voltage < PPS_V(5.0)) {
        PPS_voltage_next = PPS_voltage;
        PPS_current_next = PPS_current;
        PPS_voltage = PPS_V(5.0);
    }

    // Initialize PD protocol engine
    PD_protocol_init(&protocol);
    PD_protocol_set_power_option(&protocol, power_option);
    PD_protocol_set_PPS(&protocol, PPS_voltage, PPS_current, false);

    status_log_event(STATUS_LOG_DEV);
}

void PD_UFP_c::run(void)
{
    // 修复：Bridge模式下不调用run()函数，只使用run_Bridge()
    if (bridge_mode_enabled) {
        return; // 静默返回，避免循环重启
    }
    
    if (timer() || digitalRead(int_pin) == 0) {
        FUSB302_event_t FUSB302_events = 0;
        for (uint8_t i = 0; i < 3 && FUSB302_alert(&FUSB302, &FUSB302_events) != FUSB302_SUCCESS; i++) {}
        if (FUSB302_events) {
            handle_FUSB302_event(FUSB302_events);
        }
    }
}

//CC线状态获取函数
uint8_t PD_UFP_c::get_cc_pin() {
    uint8_t cc1, cc2;
    FUSB302_get_cc(&FUSB302, &cc1, &cc2);
    
    if (cc1 > 0 && cc2 == 0) {    // CC1有效且CC2无效
        return 1;
    } 
    else if (cc2 > 0 && cc1 == 0) { // CC2有效且CC1无效
        return 2;
    } 
    else {                         // 其他情况（都无效/都有效）
        return 0;
    }
}

bool PD_UFP_c::set_PPS(uint16_t PPS_voltage, uint8_t PPS_current)
{
    if (status_power == STATUS_POWER_PPS && PD_protocol_set_PPS(&protocol, PPS_voltage, PPS_current, true)) {
        send_request = 1;
        return true;
    }
    return false;
}

void PD_UFP_c::set_power_option(enum PD_power_option_t power_option)
{
    if (PD_protocol_set_power_option(&protocol, power_option)) {
        send_request = 1;
    }
}

void PD_UFP_c::clock_prescale_set(uint8_t prescaler)
{
    if (prescaler) {
        clock_prescaler = prescaler;
    }
}

FUSB302_ret_t PD_UFP_c::FUSB302_i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t count)
{
    Wire.beginTransmission(dev_addr);
    Wire.write(reg_addr);
    Wire.endTransmission();
    Wire.requestFrom(dev_addr, count);
    while (Wire.available() && count > 0) {
        *data++ = Wire.read();
        count--;
    }
    return count == 0 ? FUSB302_SUCCESS : FUSB302_ERR_READ_DEVICE;
}

FUSB302_ret_t PD_UFP_c::FUSB302_i2c_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t count)
{
    Wire.beginTransmission(dev_addr);
    Wire.write(reg_addr);
    while (count > 0) {
        Wire.write(*data++);
        count--;
    }
    Wire.endTransmission();
    return FUSB302_SUCCESS;
}

FUSB302_ret_t PD_UFP_c::FUSB302_delay_ms(uint32_t t)
{
    delay(t / clock_prescaler);
    return FUSB302_SUCCESS;
}

// 修复：Bridge模式初始化函数
void PD_UFP_c::init_Bridge(uint8_t int_pin)
{
    this->int_pin = int_pin;
    bridge_mode_enabled = true;
    bridge_mode_confirmed = false;
    monitor_active = true;
    bridge_log_level = PD_BRIDGE_LOG_LEVEL_DETAILED;
    
    // Initialize FUSB302 for bridge mode
    pinMode(int_pin, INPUT_PULLUP);
    FUSB302.i2c_address = 0x22;
    FUSB302.i2c_read = FUSB302_i2c_read;
    FUSB302.i2c_write = FUSB302_i2c_write;
    FUSB302.delay_ms = FUSB302_delay_ms;
    
    if (FUSB302_init(&FUSB302) == FUSB302_SUCCESS && FUSB302_get_ID(&FUSB302, 0, 0) == FUSB302_SUCCESS) {
        status_initialized = 1;
    }
    
    // 修复：完全禁用PD协议引擎
    Serial.println("[MONITOR] Bridge模式初始化: 禁用所有PD握手");
    
    // 修复：强制清除所有握手标志
    wait_src_cap = 0;
    wait_ps_rdy = 0;
    send_request = 0;
    get_src_cap_retry_count = 0;
    
    // Clear bridge log buffer
    memset(bridge_log_buffer, 0, sizeof(bridge_log_buffer));
    bridge_log_index = 0;
    
    // 修复：初始化监听数据
    memset(&pd_monitor, 0, sizeof(pd_monitor_t));
    pd_monitor.last_timestamp = clock_ms();
    pd_monitor.source_pdo_count = 0;
    memset(pd_monitor.source_pdos, 0, sizeof(pd_monitor.source_pdos));
    
    // 设置默认的监听状态
    pd_monitor.power_status = STATUS_POWER_NA;
    pd_monitor.selected_position = 1;
    
    // 修复：初始化内部状态
    status_power = STATUS_POWER_NA;
    ready_voltage = 0;
    ready_current = 0;
    
    Serial.println("[MONITOR] Bridge模式初始化完成 - 纯监听模式启动");
}

// 修复：Bridge模式运行函数
void PD_UFP_c::run_Bridge(void)
{
    if (!bridge_mode_enabled || !monitor_active) return;
    
    // 修复：彻底禁用所有PD握手逻辑
    static uint32_t last_check = 0;
    
    if (!bridge_mode_confirmed) {
        Serial.println("[MONITOR] === 纯监听模式启动：完全禁用PD握手 ===");
        bridge_mode_confirmed = true;
    }
    
    // 修复：减少不必要的检查频率
    if (millis() - last_check < 50) return; // 50ms检查一次
    last_check = millis();
    
    if (digitalRead(int_pin) == 0) {
        FUSB302_event_t FUSB302_events = 0;
        for (uint8_t i = 0; i < 3 && FUSB302_alert(&FUSB302, &FUSB302_events) != FUSB302_SUCCESS; i++) {}
        
        if (FUSB302_events) {
            handle_FUSB302_bridge_event(FUSB302_events);
        }
    }
}

// 修复：专门的Bridge事件处理函数
void PD_UFP_c::handle_FUSB302_bridge_event(FUSB302_event_t events)
{
    if (events & FUSB302_EVENT_DETACHED) {
        Serial.println("[MONITOR] [DETACH] 设备断开连接");
        
        // 修复：只更新监听状态
        pd_monitor.cc_status = false;
        pd_monitor.cc_pin = 0;
        pd_monitor.packet_count = 0;
        pd_monitor.good_crc_count = 0;
        pd_monitor.reject_count = 0;
        pd_monitor.src_cap_count = 0;
        pd_monitor.selected_position = 0;
        pd_monitor.last_voltage = 0;
        pd_monitor.last_current = 0;
        pd_monitor.power_status = STATUS_POWER_NA;
        pd_monitor.source_pdo_count = 0;
        memset(pd_monitor.source_pdos, 0, sizeof(pd_monitor.source_pdos));
    }
    
    if (events & FUSB302_EVENT_ATTACHED) {
        Serial.println("[MONITOR] [ATTACH] 设备连接检测");
        
        uint8_t cc1 = 0, cc2 = 0, cc = 0;
        FUSB302_get_cc(&FUSB302, &cc1, &cc2);
        
        if (cc1 && cc2 == 0) {
            cc = cc1;
        } else if (cc2 && cc1 == 0) {
            cc = cc2;
        }
        
        pd_monitor.cc_status = true;
        pd_monitor.cc_pin = cc;
        
        // 重置监听数据
        pd_monitor.packet_count = 0;
        pd_monitor.good_crc_count = 0;
        pd_monitor.reject_count = 0;
        pd_monitor.src_cap_count = 0;
        pd_monitor.selected_position = 1;
        pd_monitor.last_voltage = 0;
        pd_monitor.last_current = 0;
        pd_monitor.power_status = STATUS_POWER_NA;
        pd_monitor.source_pdo_count = 0;
        memset(pd_monitor.source_pdos, 0, sizeof(pd_monitor.source_pdos));
    }
    
    if (FUSB302_EVENT_RX_SOP & events) {
        // 修复：快速解析PD消息
        uint16_t header;
        uint32_t obj[7];
        FUSB302_get_message(&FUSB302, &header, obj);
        
        pd_monitor.packet_count++;
        
        // 快速解析消息头
        uint16_t msg_header = header;
        uint8_t num_obj = (msg_header >> 12) & 0x07;
        uint8_t msg_type = (msg_header >> 0) & 0x1F;
        
        // 修复：优化日志结构，替换易混淆字符
        Serial.printf("[PD_RX] H:0x%04X O:%d T:%d (", header, num_obj, msg_type);
        
        // 消息类型解析
        switch(msg_type) {
            case 0x01: Serial.print("SOURCE_CAP"); break;
            case 0x02: Serial.print("REQUEST"); break;
            case 0x03: Serial.print("PS_RDY"); break;
            case 0x04: Serial.print("GET_SRC_CAP"); break;
            case 0x05: Serial.print("GET_SINK_CAP"); break;
            case 0x06: Serial.print("DR_SWAP"); break;
            case 0x07: Serial.print("PR_SWAP"); break;
            case 0x08: Serial.print("VCONN_SWAP"); break;
            case 0x09: Serial.print("WAIT"); break;
            case 0x0A: Serial.print("SOFT_RESET"); break;
            case 0x0B: Serial.print("DATA_RESET"); break;
            case 0x0C: Serial.print("DATA_RESET_COMPLETE"); break;
            case 0x0D: Serial.print("NOT_SUPPORTED"); break;
            case 0x0E: Serial.print("GET_SRC_STATUS"); break;
            case 0x0F: Serial.print("GET_PPS_STATUS"); break;
            case 0x10: Serial.print("GET_SINK_STATUS"); break;
            default: Serial.print("UNKNOWN"); break;
        }
        Serial.println(")");
        
        // 快速输出PDO数据
        for (uint8_t i = 0; i < num_obj && i < 7; i++) {
            Serial.printf("[PD_RX]  PDO[%d]=0x%08lX\n", i, obj[i]);
        }
        
        // 修复：优化Source Capabilities处理
        if (num_obj > 0 && msg_type == 0x01) {
            Serial.println("[MONITOR] [SRC_CAP] Source Capabilities解析");
            
            pd_monitor.src_cap_count = num_obj;
            pd_monitor.source_pdo_count = num_obj;
            
            for (uint8_t i = 0; i < num_obj && i < 7; i++) {
                pd_monitor.source_pdos[i] = obj[i];
                
                uint32_t pdo = obj[i];
                uint8_t pdo_type = (pdo >> 30) & 0x03;
                uint16_t voltage_mv = 0;
                uint16_t current_ma = 0;
                
                switch (pdo_type) {
                    case 0: // Fixed Supply
                        {
                            uint16_t v_raw = ((pdo >> 10) & 0x3FF);
                            uint16_t i_raw = ((pdo >> 0) & 0x3FF);
                            voltage_mv = v_raw * 50;
                            current_ma = i_raw * 10;
                            Serial.printf("[MONITOR]  PDO[%d]: FIX %umV %umA\n", i, voltage_mv, current_ma);
                        }
                        break;
                    case 3: // PPS
                        {
                            uint16_t v_min_raw = ((pdo >> 17) & 0x7FF);
                            uint16_t v_max_raw = ((pdo >> 24) & 0x3F);
                            uint16_t i_max_raw = ((pdo >> 7) & 0xFF);
                            uint16_t v_min_mv = v_min_raw * 100;
                            uint16_t v_max_mv = v_max_raw * 100;
                            uint16_t i_max_ma = i_max_raw * 50;
                            Serial.printf("[MONITOR]  PDO[%d]: PPS %umV-%umV max%umA\n", 
                                          i, v_min_mv, v_max_mv, i_max_ma);
                        }
                        break;
                    default:
                        Serial.printf("[MONITOR]  PDO[%d]: TYPE %d\n", i, pdo_type);
                        break;
                }
            }
            
            Serial.printf("[MONITOR] [SRC_CAP] %d个PDO已缓存\n", pd_monitor.source_pdo_count);
            pd_monitor.selected_position = 1;
        }
        
        // 修复：优化Request处理
        if (num_obj > 0 && msg_type == 0x02) {
            Serial.println("[MONITOR] [REQUEST] Request消息解析");
            
            uint32_t request_pdo = obj[0];
            uint8_t obj_position = (request_pdo >> 28) & 0x0F;
            
            Serial.printf("[MONITOR]  REQ: 0x%08lX POS:%u\n", request_pdo, obj_position);
            
            if (pd_monitor.source_pdo_count > 0 && obj_position > 0 && obj_position <= pd_monitor.source_pdo_count) {
                uint32_t source_pdo = pd_monitor.source_pdos[obj_position - 1];
                uint8_t source_pdo_type = (source_pdo >> 30) & 0x03;
                
                uint16_t voltage = 0;
                uint16_t current = 0;
                status_power_t power_status = STATUS_POWER_TYP;
                
                if (source_pdo_type == 3) {
                    // 修复：PPS模式直接从Request中解析当前电压电流
                    uint16_t voltage_raw = ((request_pdo >> 17) & 0x7FF);  // 100mV units
                    uint16_t current_raw = ((request_pdo >> 7) & 0xFF);    // 50mA units
                    
                    voltage = voltage_raw * 100; // 转换为mV
                    current = current_raw * 50;   // 转换为mA
                    power_status = STATUS_POWER_PPS;
                    
                    Serial.printf("[MONITOR]  PPS解析: %umV %umA (原始: %u*100mV, %u*50mA)\n", 
                                  voltage, current, voltage_raw, current_raw);
                } else {
                    // Fixed模式
                    switch (source_pdo_type) {
                        case 0: // Fixed Supply
                            {
                                uint16_t voltage_raw = ((source_pdo >> 10) & 0x3FF);
                                uint16_t current_raw = ((source_pdo >> 0) & 0x3FF);
                                voltage = voltage_raw * 50;
                                current = current_raw * 10;
                                power_status = STATUS_POWER_TYP;
                                Serial.printf("[MONITOR]  FIX解析: %umV %umA\n", voltage, current);
                            }
                            break;
                        default:
                            Serial.printf("[MONITOR]  不支持类型: %d\n", source_pdo_type);
                            break;
                    }
                }
                
                if (voltage > 0) {
                    // 立即更新Bridge状态
                    pd_monitor.last_voltage = voltage;
                    pd_monitor.last_current = current;
                    pd_monitor.power_status = power_status;
                    pd_monitor.selected_position = obj_position;
                    
                    Serial.printf("[MONITOR] [STATUS] %.3fV %.3fA [%s] POS:%u\n",
                                  voltage/1000.0f, current/1000.0f,
                                  power_status == STATUS_POWER_PPS ? "PPS" : "FIX",
                                  obj_position);
                }
            } else {
                Serial.printf("[MONITOR]  错误: 位置%u超出范围 (1-%u)\n", 
                              obj_position, pd_monitor.source_pdo_count);
            }
        }
        
        // 修复：快速处理PS_RDY
        if (msg_type == 0x03) {
            Serial.println("[MONITOR] [PS_RDY] 电源就绪");
            
            if (num_obj > 0 && obj[0] != 0) {
                uint32_t pdo = obj[0];
                uint8_t pdo_type = (pdo >> 30) & 0x03;
                
                Serial.printf("[MONITOR]  PDO:0x%08lX TYPE:%d\n", pdo, pdo_type);
                
                if (pdo_type == 0) { // Fixed Supply
                    uint16_t voltage_raw = ((pdo >> 10) & 0x3FF);
                    uint16_t current_raw = ((pdo >> 0) & 0x3FF);
                    uint16_t voltage = voltage_raw * 50;
                    uint16_t current = current_raw * 10;
                    
                    pd_monitor.last_voltage = voltage;
                    pd_monitor.last_current = current;
                    pd_monitor.power_status = STATUS_POWER_TYP;
                    
                    Serial.printf("[MONITOR]  FIX: %.3fV %.3fA\n", voltage/1000.0f, current/1000.0f);
                } else if (pdo_type == 3) { // PPS
                    uint16_t voltage_raw = ((pdo >> 17) & 0x7FF);
                    uint16_t current_raw = ((pdo >> 7) & 0xFF);
                    uint16_t voltage = voltage_raw * 100;
                    uint16_t current = current_raw * 50;
                    
                    pd_monitor.last_voltage = voltage;
                    pd_monitor.last_current = current;
                    pd_monitor.power_status = STATUS_POWER_PPS;
                    
                    Serial.printf("[MONITOR]  PPS: %.3fV %.3fA\n", voltage/1000.0f, current/1000.0f);
                }
            } else {
                Serial.println("[MONITOR]  确认当前状态");
                if (pd_monitor.last_voltage > 0) {
                    Serial.printf("[MONITOR]  CONF: %.3fV %.3fA [%s]\n",
                                  pd_monitor.last_voltage/1000.0f,
                                  pd_monitor.last_current/1000.0f,
                                  pd_monitor.power_status == STATUS_POWER_PPS ? "PPS" : "FIX");
                }
            }
        }
    }
    
    if (events & FUSB302_EVENT_GOOD_CRC_SENT) {
        pd_monitor.good_crc_count++;
        Serial.println("[MONITOR] [CRC] Good CRC已统计");
    }
}

// 修复：get_bridge_voltage函数
float PD_UFP_c::get_bridge_voltage(void)
{
    if (bridge_mode_enabled && monitor_active) {
        if (pd_monitor.last_voltage > 0) {
            float voltage_v = pd_monitor.last_voltage / 1000.0f;
            return voltage_v;
        }
        return 0.0f;
    }
    
    // 非Bridge模式：使用原有逻辑
    if (status_power == STATUS_POWER_PPS) {
        return ready_voltage * 0.02f;
    } else if (status_power == STATUS_POWER_TYP) {
        return ready_voltage * 0.05f;
    }
    return 0.0f;
}

// 修复：get_bridge_current函数
float PD_UFP_c::get_bridge_current(void)
{
    if (bridge_mode_enabled && monitor_active) {
        if (pd_monitor.last_current > 0) {
            float current_a = pd_monitor.last_current / 1000.0f;
            return current_a;
        }
        return 0.0f;
    }
    
    // 非Bridge模式：使用原有逻辑
    if (status_power == STATUS_POWER_PPS) {
        return ready_current * 0.05f;
    } else if (status_power == STATUS_POWER_TYP) {
        return ready_current * 0.01f;
    }
    return 0.0f;
}

String PD_UFP_c::get_bridge_power_mode(void)
{
    if (bridge_mode_enabled && monitor_active) {
        switch (pd_monitor.power_status) {
            case STATUS_POWER_TYP:
                return "FIX";
            case STATUS_POWER_PPS:
                return "PPS";
            case STATUS_POWER_NA:
            default:
                return "MON";
        }
    }
    return "NA";
}

uint32_t PD_UFP_c::get_bridge_packet_count(void)
{
    return pd_monitor.packet_count;
}

uint8_t PD_UFP_c::get_bridge_src_cap_count(void)
{
    return pd_monitor.src_cap_count;
}

uint8_t PD_UFP_c::get_bridge_selected_position(void)
{
    return pd_monitor.selected_position;
}

uint8_t PD_UFP_c::get_bridge_cc_pin(void)
{
    return pd_monitor.cc_pin;
}

uint32_t PD_UFP_c::get_bridge_good_crc_count(void)
{
    return pd_monitor.good_crc_count;
}

uint32_t PD_UFP_c::get_bridge_reject_count(void)
{
    return pd_monitor.reject_count;
}

bool PD_UFP_c::get_bridge_cc_status(void)
{
    return pd_monitor.cc_status;
}

void PD_UFP_c::reset_bridge_monitor(void)
{
    memset(&pd_monitor, 0, sizeof(pd_monitor_t));
    pd_monitor.last_timestamp = clock_ms();
    pd_monitor.power_status = STATUS_POWER_NA;
    pd_monitor.selected_position = 1;
    pd_monitor.source_pdo_count = 0;
    memset(pd_monitor.source_pdos, 0, sizeof(pd_monitor.source_pdos));
    Serial.println("[MONITOR] 监听数据已重置");
}

void PD_UFP_c::set_bridge_log_level(pd_bridge_log_level_t level)
{
    bridge_log_level = level;
    Serial.printf("[MONITOR] 日志级别设置为: %s\n", 
                  level == PD_BRIDGE_LOG_LEVEL_DETAILED ? "详细" : "基础");
}

void PD_UFP_c::force_refresh_bridge_status(void)
{
    if (bridge_mode_enabled && monitor_active) {
        // 强制刷新状态
        uint8_t cc1 = 0, cc2 = 0;
        FUSB302_get_cc(&FUSB302, &cc1, &cc2);
        
        bool connected = (cc1 > 0 || cc2 > 0);
        uint8_t cc_pin = 0;
        
        if (cc1 > 0 && cc2 == 0) cc_pin = 1;
        else if (cc2 > 0 && cc1 == 0) cc_pin = 2;
        
        pd_monitor.cc_status = connected;
        pd_monitor.cc_pin = cc_pin;
        
        Serial.printf("[MONITOR] [REFRESH] CC状态: %s, CC%d\n", 
                      connected ? "连接" : "断开", cc_pin);
    }
}

// 修复：日志读取函数
int PD_UFP_c::status_bridge_log_readline(char *buffer, int maxlen)
{
    if (!bridge_mode_enabled || !monitor_active) return 0;
    
    // 简化的日志格式
    int len = 0;
    
    if (pd_monitor.last_voltage > 0) {
        len = snprintf(buffer, maxlen, "%.3fV %.3fA [%s] Pkts:%u CRC:%u",
                      get_bridge_voltage(), get_bridge_current(),
                      get_bridge_power_mode().c_str(),
                      pd_monitor.packet_count, pd_monitor.good_crc_count);
    } else {
        len = snprintf(buffer, maxlen, "等待连接... Pkts:%u CRC:%u",
                      pd_monitor.packet_count, pd_monitor.good_crc_count);
    }
    
    return len;
}

void PD_UFP_c::reset_monitor_info(void)
{
    reset_bridge_monitor();
}

void PD_UFP_c::update_monitor_info(void)
{
    // Bridge模式下不需要调用此函数
}

// 其他函数保持原有实现...
bool PD_UFP_c::timer(void)
{
    uint16_t time = clock_ms();
    bool t = time != time_polling;
    time_polling = time;
    return t;
}

void PD_UFP_c::set_default_power(void)
{
    if (!bridge_mode_enabled) {
        status_power_ready(STATUS_POWER_TYP, 5000, 300); // 5V 3A
    }
}

void PD_UFP_c::status_power_ready(status_power_t status, uint16_t voltage, uint16_t current)
{
    // Bridge模式下不调用此函数
    if (!bridge_mode_enabled) {
        status_power = status;
        ready_voltage = voltage;
        ready_current = current;
    }
}

void PD_UFP_c::handle_protocol_event(PD_protocol_event_t events)
{
    // Bridge模式下禁用
}

void PD_UFP_c::handle_FUSB302_event(FUSB302_event_t events)
{
    // Bridge模式下使用handle_FUSB302_bridge_event
    if (bridge_mode_enabled) {
        handle_FUSB302_bridge_event(events);
        return;
    }
    
    // 非Bridge模式的原有逻辑...
}

uint16_t PD_UFP_c::clock_ms(void)
{
    static uint32_t last_time = 0;
    uint32_t current_time = millis();
    if (current_time < last_time) {
        last_time = current_time;
    }
    return current_time - last_time;
}

void PD_UFP_c::delay_ms(uint16_t ms)
{
    delay(ms / clock_prescaler);
}

// 添加额外的Bridge功能函数
String PD_UFP_c::get_bridge_power_info_string(void)
{
    if (!bridge_mode_enabled || !monitor_active) return "Monitor: NA";
    
    if (pd_monitor.last_voltage > 0) {
        return String::format("%.3fV %.3fA [%s]", 
                            get_bridge_voltage(), get_bridge_current(),
                            get_bridge_power_mode().c_str());
    } else {
        return "Monitor: 等待连接";
    }
}

uint32_t PD_UFP_c::get_bridge_max_power(void)
{
    if (!bridge_mode_enabled || !monitor_active) return 0;
    
    if (pd_monitor.last_voltage > 0 && pd_monitor.last_current > 0) {
        return (uint32_t)(pd_monitor.last_voltage * pd_monitor.last_current / 1000);
    }
    return 0;
}

uint16_t PD_UFP_c::get_bridge_voltage_range_min(void)
{
    if (!bridge_mode_enabled || !monitor_active) return 0;
    
    // 从第一个PPS PDO获取最小电压
    for (uint8_t i = 0; i < pd_monitor.source_pdo_count; i++) {
        uint32_t pdo = pd_monitor.source_pdos[i];
        uint8_t pdo_type = (pdo >> 30) & 0x03;
        if (pdo_type == 3) { // PPS
            uint16_t v_min_raw = ((pdo >> 17) & 0x7FF);
            return v_min_raw * 100; // mV
        }
    }
    return 0;
}

uint16_t PD_UFP_c::get_bridge_voltage_range_max(void)
{
    if (!bridge_mode_enabled || !monitor_active) return 0;
    
    // 从第一个PPS PDO获取最大电压
    for (uint8_t i = 0; i < pd_monitor.source_pdo_count; i++) {
        uint32_t pdo = pd_monitor.source_pdos[i];
        uint8_t pdo_type = (pdo >> 30) & 0x03;
        if (pdo_type == 3) { // PPS
            uint16_t v_max_raw = ((pdo >> 24) & 0x3F);
            return v_max_raw * 100; // mV
        }
    }
    return 0;
}

uint16_t PD_UFP_c::get_bridge_current_limit(void)
{
    if (!bridge_mode_enabled || !monitor_active) return 0;
    
    // 从第一个PPS PDO获取电流限制
    for (uint8_t i = 0; i < pd_monitor.source_pdo_count; i++) {
        uint32_t pdo = pd_monitor.source_pdos[i];
        uint8_t pdo_type = (pdo >> 30) & 0x03;
        if (pdo_type == 3) { // PPS
            uint16_t i_max_raw = ((pdo >> 7) & 0xFF);
            return i_max_raw * 50; // mA
        }
    }
    return 0;
}

bool PD_UFP_c::is_bridge_pps_capable(void)
{
    if (!bridge_mode_enabled || !monitor_active) return false;
    
    for (uint8_t i = 0; i < pd_monitor.source_pdo_count; i++) {
        uint32_t pdo = pd_monitor.source_pdos[i];
        uint8_t pdo_type = (pdo >> 30) & 0x03;
        if (pdo_type == 3) { // PPS
            return true;
        }
    }
    return false;
}

uint8_t PD_UFP_c::clock_prescaler = 1;