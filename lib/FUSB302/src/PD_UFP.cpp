/**
 * PD_UFP.cpp
 *
 *      Author: Ryan Ma
 *      Edited: Kai Liebich
 *      v1.3: 极简Bridge模式优化版本
 *
 * Minimalist USB PD Ardunio Library for PD Micro board
 * Only support UFP(device) sink only functionality
 * Requires FUSB302_UFP.h, PD_UFP_Protocol.h and Standard Arduino Library
 *
 * Support PD3.0 PPS
 * 
 * v1.3优化内容：
 * - 完全移除PD缓存机制，避免数据损坏和映射错误
 * - 直接解析PD消息，无需复杂的缓存查找逻辑
 * - 极简日志输出，大幅减少串口压力和缓冲溢出
 * - 实时状态更新，操作与显示完全对应
 * - 优化性能，避免高频日志导致的性能问题
 * - 简化代码结构，提高可维护性
 */

#include <stdint.h>
#include <string.h>

#include "PD_UFP.h"

// Optimize RAM usage on AVR MCU by allocate format string in program memory
#if defined(__AVR__)
#include <avr/pgmspace.h>
#define SNPRINTF snprintf_P
#define MY_PSTR(str) PSTR(str)
#else
#define SNPRINTF snprintf
#define MY_PSTR(str) str
#endif

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

// Status log mask definitions for circular buffer operations
#define STATUS_LOG_MASK         15  // 16 entries (2^4)
#define STATUS_LOG_OBJ_MASK     15  // 16 entries (2^4)

///////////////////////////////////////////////////////////////////////////////////////////////////
// PD_UFP_c
///////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t PD_UFP_c::clock_prescaler = 1;

PD_UFP_c::PD_UFP_c()
{
    // v1.3优化：简化初始化，只初始化核心变量
    bridge_mode_enabled = false;
    bridge_log_index = 0;
    bridge_log_level = PD_BRIDGE_LOG_LEVEL_BASIC; // 默认基础模式，用户可在初始化后修改
    
    // 初始化基本状态
    status_initialized = 0;
    status_src_cap_received = 0;
    status_power = STATUS_POWER_NA;
    ready_voltage = 0;
    ready_current = 0;
    
    // 重置监听数据
    memset(&pd_monitor, 0, sizeof(pd_monitor_t));
    pd_monitor.last_timestamp = clock_ms();
}

void PD_UFP_c::init(uint8_t int_pin, enum PD_power_option_t power_option)
{
    // v1.3优化：正常PD模式初始化，保持原有逻辑不变
    this->int_pin = int_pin;
    
    // Initialize FUSB302
    pinMode(int_pin, INPUT_PULLUP);
    FUSB302.i2c_address = 0x22;
    FUSB302.i2c_read = FUSB302_i2c_read;
    FUSB302.i2c_write = FUSB302_i2c_write;
    FUSB302.delay_ms = FUSB302_delay_ms;
    
    if (FUSB302_init(&FUSB302) == FUSB302_SUCCESS && FUSB302_get_ID(&FUSB302, 0, 0) == FUSB302_SUCCESS) {
        status_initialized = 1;
    }
    
    // Initialize PD protocol engine
    PD_protocol_init(&protocol);
    PD_protocol_set_power_option(&protocol, power_option);
    PD_protocol_set_PPS(&protocol, 0, 0, false);

    status_log_event(STATUS_LOG_DEV);
}

void PD_UFP_c::init_PPS(uint8_t int_pin, uint16_t PPS_voltage, uint8_t PPS_current, enum PD_power_option_t power_option)
{
    this->int_pin = int_pin;
    
    // Initialize FUSB302
    pinMode(int_pin, INPUT_PULLUP);
    FUSB302.i2c_address = 0x22;
    FUSB302.i2c_read = FUSB302_i2c_read;
    FUSB302.i2c_write = FUSB302_i2c_write;
    FUSB302.delay_ms = FUSB302_delay_ms;
    
    if (FUSB302_init(&FUSB302) == FUSB302_SUCCESS && FUSB302_get_ID(&FUSB302, 0, 0) == FUSB302_SUCCESS) {
        status_initialized = 1;
    }
    
    // Initialize PD protocol engine
    PD_protocol_init(&protocol);
    PD_protocol_set_power_option(&protocol, power_option);
    PD_protocol_set_PPS(&protocol, PPS_voltage, PPS_current, false);

    status_log_event(STATUS_LOG_DEV);
}

bool PD_UFP_c::enable_vbus_sense(bool enable)
{
    return FUSB302_set_vbus_sense(&FUSB302, enable);
}

void PD_UFP_c::run(void)
{
    // v1.3优化：Bridge模式下不调用run()函数，只使用run_Bridge()
    if (bridge_mode_enabled) {
        return;
    }
    
    if (status_initialized) {
        FUSB302_event_t FUSB302_events = 0;
        if (FUSB302_alert(&FUSB302, &FUSB302_events) == FUSB302_SUCCESS) {
            if (FUSB302_events) {
                handle_FUSB302_event(FUSB302_events);
            }
        }
        
        if (timer()) {
            // timer callback
        }
    }
}

void PD_UFP_c::handle_protocol_event(PD_protocol_event_t events)
{
    // v1.3优化：保持原有逻辑不变，只在非Bridge模式下使用
    if (events & PD_PROTOCOL_EVENT_SRC_CAP) {
        wait_src_cap = 0;
        get_src_cap_retry_count = 0;
        wait_ps_rdy = 1;
        time_wait_ps_rdy = clock_ms();
        status_log_event(STATUS_LOG_SRC_CAP);
    }
    if (events & PD_PROTOCOL_EVENT_REJECT) {
        if (wait_ps_rdy) {
            wait_ps_rdy = 0;
            status_log_event(STATUS_LOG_POWER_REJECT);
        }
    }    
    if (events & PD_PROTOCOL_EVENT_PS_RDY) {
        // 非Bridge模式：正常处理PS_RDY事件
        PD_power_info_t p;
        uint8_t i, selected_power = PD_protocol_get_selected_power(&protocol);
        PD_protocol_get_power_info(&protocol, selected_power, &p);
        wait_ps_rdy = 0;
        if (p.type == PD_PDO_TYPE_AUGMENTED_PDO) {
            // PPS mode
            if (PPS_voltage_next) {
                // Two stage startup for PPS voltage < 5V
                PD_protocol_set_PPS(&protocol, PPS_voltage_next, PPS_current_next, false);
                PPS_voltage_next = 0;
                send_request = 1;
                status_log_event(STATUS_LOG_POWER_PPS_STARTUP);
            } else {
                time_PPS_request = clock_ms();
                status_power_ready(STATUS_POWER_PPS, 
                    PD_protocol_get_PPS_voltage(&protocol), PD_protocol_get_PPS_current(&protocol));
                status_log_event(STATUS_LOG_POWER_READY);
            }
        } else {
            FUSB302_set_vbus_sense(&FUSB302, 1);
            status_power_ready(STATUS_POWER_TYP, p.max_v, p.max_i);
            status_log_event(STATUS_LOG_POWER_READY);
        }
    }
}

void PD_UFP_c::handle_FUSB302_event(FUSB302_event_t events)
{
    if (events & FUSB302_EVENT_DETACHED) {
        // v1.3优化：Bridge模式下不调用PD_protocol_reset
        if (!bridge_mode_enabled) {
            PD_protocol_reset(&protocol);
        }
        
        if (bridge_mode_enabled) {
            pd_monitor.cc_status = false;
            pd_monitor.cc_pin = 0;
        }
        
        status_log_event(STATUS_LOG_CC);
    }
    if (events & FUSB302_EVENT_ATTACHED) {
        uint8_t cc1 = 0, cc2 = 0, cc = 0;
        FUSB302_get_cc(&FUSB302, &cc1, &cc2);
        
        if (cc1 && cc2 == 0) {
            cc = cc1;
        } else if (cc2 && cc1 == 0) {
            cc = cc2;
        }
        
        if (bridge_mode_enabled) {
            pd_monitor.cc_status = true;
            pd_monitor.cc_pin = cc;
        }
        
        status_log_event(STATUS_LOG_CC);
    }
    if (events & FUSB302_EVENT_RX_SOP) {
        uint16_t header;
        uint32_t obj[7];
        FUSB302_get_message(&FUSB302, &header, obj);
        
        // v1.3优化：Bridge模式下的数据包计数
        if (bridge_mode_enabled) {
            pd_monitor.packet_count++;
        }
        
        // v1.3优化：Bridge模式下完全不调用任何PD协议函数
        if (!bridge_mode_enabled) {
            // 非Bridge模式：正常PD协议处理
            PD_protocol_event_t protocol_event = 0;
            PD_protocol_handle_msg(&protocol, header, obj, &protocol_event);
            status_log_event(STATUS_LOG_MSG_RX, obj);
            if (protocol_event) {
                handle_protocol_event(protocol_event);
            }
        } else {
            // Bridge模式：只记录消息，完全不进行协议处理
            status_log_event(STATUS_LOG_MSG_RX, obj);
        }
    }
    if (events & FUSB302_EVENT_GOOD_CRC_SENT) {
        // v1.3优化：Bridge模式下的Good CRC计数
        if (bridge_mode_enabled) {
            pd_monitor.good_crc_count++;
        } else {
            // 非Bridge模式：正常PD协议响应
            uint16_t header;
            uint32_t obj[7];
            delay_ms(2);  /* Delay respond in case there are retry messages */
            if (PD_protocol_respond(&protocol, &header, obj)) {
                status_log_event(STATUS_LOG_MSG_TX, obj);
                FUSB302_tx_sop(&FUSB302, header, obj);
            }
        }
    }
}

bool PD_UFP_c::timer(void)
{
    // v1.3优化：Bridge模式下完全禁用timer功能
    if (bridge_mode_enabled) {
        return false;
    }
    
    uint16_t t = clock_ms();
    
    if (wait_src_cap) {
        if (t - time_wait_src_cap > t_TypeCSinkWaitCap) {
            if (get_src_cap_retry_count < 3) {
                uint16_t header;
                get_src_cap_retry_count += 1;
                /* Try to request source capabilities message (will not cause power cycle VBUS) */
                PD_protocol_create_get_src_cap(&protocol, &header);
                status_log_event(STATUS_LOG_MSG_TX);
                FUSB302_tx_sop(&FUSB302, header, 0);
            } else {
                get_src_cap_retry_count = 0;
                /* Hard reset will cause the source power cycle VBUS. */
                FUSB302_tx_hard_reset(&FUSB302);
                PD_protocol_reset(&protocol);
            }
        }
    }
    if (wait_ps_rdy) {
        if (t - time_wait_ps_rdy > t_RequestToPSReady) {
            wait_ps_rdy = 0;
            set_default_power();
        }
    }
    if (send_request) {
        time_PPS_request = t;
        uint16_t header;
        uint32_t obj[7];
        /* Send request if option updated or regularly in PPS mode to keep power alive */
        PD_protocol_create_request(&protocol, &header, obj);
        status_log_event(STATUS_LOG_MSG_TX, obj);
        time_wait_ps_rdy = clock_ms();
        FUSB302_tx_sop(&FUSB302, header, obj);
    }
    if (t - time_polling > t_PD_POLLING) {
        time_polling = t;
        return true;
    }
    return false;
}

void PD_UFP_c::set_default_power(void)
{
    status_power_ready(STATUS_POWER_TYP, PD_V(5), PD_A(1));
    status_log_event(STATUS_LOG_POWER_READY);
}

void PD_UFP_c::status_power_ready(status_power_t status, uint16_t voltage, uint16_t current)
{
    // v1.3优化：Bridge模式下不调用status_power_ready，避免协议状态更新
    if (bridge_mode_enabled) {
        return;
    }
    
    status_power = status;
    ready_voltage = voltage;
    ready_current = current;
    
    if (status == STATUS_POWER_TYP) {
        FUSB302_set_vbus_sense(&FUSB302, 1);
    }
}

void PD_UFP_c::delay_ms(uint16_t ms)
{
    delay(ms);
}

uint16_t PD_UFP_c::clock_ms(void)
{
    return (uint16_t)millis() * clock_prescaler;
}

void PD_UFP_c::update_monitor_info(void)
{
    // v1.3优化：简化状态更新逻辑
    pd_monitor.last_timestamp = clock_ms();
    
    if (bridge_mode_enabled) {
        // Bridge模式：只更新时间戳，不访问协议状态
        // 所有状态更新都在run_Bridge()中的事件处理里完成
        return;
    } else {
        // 非Bridge模式：正常更新所有信息
        pd_monitor.last_voltage = get_voltage();
        pd_monitor.last_current = get_current();
        pd_monitor.power_status = get_ps_status();
        pd_monitor.cc_pin = get_cc_pin();
        pd_monitor.cc_status = (pd_monitor.cc_pin != 0);
        pd_monitor.src_cap_count = get_src_cap_count();
        pd_monitor.selected_position = get_selected_position();
    }
}

void PD_UFP_c::reset_monitor_info(void)
{
    memset(&pd_monitor, 0, sizeof(pd_monitor_t));
    pd_monitor.last_timestamp = clock_ms();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// v1.3优化：Bridge功能方法实现 - 极简版本
///////////////////////////////////////////////////////////////////////////////////////////////////

void PD_UFP_c::init_Bridge(uint8_t int_pin)
{
    this->int_pin = int_pin;
    bridge_mode_enabled = true;
    // v1.3修复：不强制设置日志级别，允许用户自定义
    // 注意：如需显示PKT计数器，请在init_Bridge后调用 set_bridge_log_level(PD_BRIDGE_LOG_LEVEL_DETAILED);
    // bridge_log_level 保持原有设置，如果未设置则默认为基础模式
    
    // Initialize FUSB302 for bridge mode
    pinMode(int_pin, INPUT_PULLUP); // Set FUSB302 int pin input and pull up
    FUSB302.i2c_address = 0x22;
    FUSB302.i2c_read = FUSB302_i2c_read;
    FUSB302.i2c_write = FUSB302_i2c_write;
    FUSB302.delay_ms = FUSB302_delay_ms;
    
    if (FUSB302_init(&FUSB302) == FUSB302_SUCCESS && FUSB302_get_ID(&FUSB302, 0, 0) == FUSB302_SUCCESS) {
        status_initialized = 1;
    }
    
    // v1.3优化：Bridge模式完全不初始化PD协议引擎
    // 禁用所有PD协议处理
    Serial.println("[BRIDGE] Bridge模式：禁用PD协议引擎，纯监听模式");
    
    // v1.3优化：强制清除所有可能触发握手的标志
    wait_src_cap = 0;
    wait_ps_rdy = 0;
    send_request = 0;
    get_src_cap_retry_count = 0;
    
    // v1.3优化：初始化监听数据
    memset(&pd_monitor, 0, sizeof(pd_monitor_t));
    pd_monitor.last_timestamp = clock_ms();
    
    // 设置默认的监听状态
    pd_monitor.power_status = STATUS_POWER_NA;  // 未知电源状态
    pd_monitor.selected_position = 1;  // 默认选择第一个电源（位置1）
    
    pd_monitor.last_voltage = 5000;    // 5V默认电压（mV）
    pd_monitor.last_current = 1000;    // 1A默认电流（mA）
    // v1.3优化：初始化内部状态
    status_power = STATUS_POWER_NA;  // 确保内部状态正确初始化
    ready_voltage = 0;
    ready_current = 0;
    
    Serial.println("[BRIDGE] Bridge模式初始化完成");
}

void PD_UFP_c::run_Bridge(void)
{
    if (!bridge_mode_enabled) return;
    
    // v1.3优化：极简Bridge模式：无缓存，直接解析，最小日志输出
    static bool bridge_mode_confirmed = false;
    if (!bridge_mode_confirmed) {
        Serial.println("[BRIDGE] Bridge模式启动");
        bridge_mode_confirmed = true;
    }
    
    if (digitalRead(int_pin) == 0) {
        FUSB302_event_t FUSB302_events = 0;
        for (uint8_t i = 0; i < 3 && FUSB302_alert(&FUSB302, &FUSB302_events) != FUSB302_SUCCESS; i++) {}
        
        if (FUSB302_events) {
            if (FUSB302_events & FUSB302_EVENT_DETACHED) {
                // 设备断开
                pd_monitor.cc_status = false;
                pd_monitor.cc_pin = 0;
                pd_monitor.packet_count = 0;
                pd_monitor.good_crc_count = 0;
                pd_monitor.reject_count = 0;
                pd_monitor.src_cap_count = 0;
                pd_monitor.source_pdo_count = 0;  // 清空PDO缓存
                pd_monitor.selected_position = 0;
                pd_monitor.last_voltage = 0;
                pd_monitor.last_current = 0;
                pd_monitor.power_status = STATUS_POWER_NA;
                
                // v1.3新增：清空最新PDO消息存储
                pd_monitor.last_msg_header = 0;
                pd_monitor.last_msg_obj_count = 0;
                pd_monitor.last_msg_type = 0;
            }
            
            if (FUSB302_events & FUSB302_EVENT_ATTACHED) {
                // 设备连接
                uint8_t cc1 = 0, cc2 = 0, cc = 0;
                FUSB302_get_cc(&FUSB302, &cc1, &cc2);
                
                // 确保cc1和cc2是有效值（0-2），防止异常值导致CC显示3
                cc1 = (cc1 > 2) ? 0 : cc1;
                cc2 = (cc2 > 2) ? 0 : cc2;
                
                // 完整的CC引脚赋值逻辑
                if (cc1 > 0 && cc2 == 0) {
                    cc = cc1;      // CC1有效
                } else if (cc2 > 0 && cc1 == 0) {
                    cc = cc2;      // CC2有效
                } else if (cc1 > 0 && cc2 > 0) {
                    cc = 1;        // 两边都有效，默认选择CC1
                } else {
                    cc = 0;        // 两边都无效
                }
                
                pd_monitor.cc_status = true;
                pd_monitor.cc_pin = cc;
                pd_monitor.packet_count = 0;
                pd_monitor.good_crc_count = 0;
                pd_monitor.reject_count = 0;
                pd_monitor.src_cap_count = 0;
                pd_monitor.source_pdo_count = 0;  // 重置PDO缓存
                pd_monitor.selected_position = 1;
                pd_monitor.last_voltage = 0;
                pd_monitor.last_current = 0;
                pd_monitor.power_status = STATUS_POWER_NA;
                
                // v1.3新增：重置最新PDO消息存储
                pd_monitor.last_msg_header = 0;
                pd_monitor.last_msg_obj_count = 0;
                pd_monitor.last_msg_type = 0;
                
                // v1.3修复：设备连接时推测可能的电源模式
                // 等待Source Capabilities消息来确定具体模式
            }
            
            if (FUSB302_EVENT_RX_SOP & FUSB302_events) {
                // 接收PD消息，直接解析不缓存
                uint16_t header;
                uint32_t obj[7];
                FUSB302_get_message(&FUSB302, &header, obj);
                
                pd_monitor.packet_count++;
                
                uint8_t num_obj = (header >> 12) & 0x07;
                uint8_t msg_type = (header >> 0) & 0x1F;
                
                // v1.3新增：存储最新PDO消息数据用于TFT显示
                pd_monitor.last_msg_header = header;
                pd_monitor.last_msg_obj_count = num_obj;
                pd_monitor.last_msg_type = msg_type;
                for (uint8_t i = 0; i < 7; i++) {  // 初始化所有7个位置
                    if (i < num_obj && i < 7) {
                        pd_monitor.last_msg_obj[i] = obj[i];
                    } else {
                        pd_monitor.last_msg_obj[i] = 0;  // 清空未使用的位置
                    }
                }
                
                // v1.3恢复：完整PD消息解析逻辑
                
                if (num_obj > 0 && msg_type == 0x01) {
                    // Source Capabilities - 缓存所有PDO并解析
                    pd_monitor.src_cap_count = num_obj;
                    pd_monitor.source_pdo_count = num_obj;
                    
                    for (uint8_t i = 0; i < num_obj && i < 7; i++) {
                        pd_monitor.source_pdos[i] = obj[i];
                        
                        // 快速解析PDO信息
                        uint32_t pdo = obj[i];
                        uint8_t pdo_type = (pdo >> 30) & 0x03;
                        
                        if (pdo_type == 0) { // Fixed Supply
                            uint16_t v_raw = ((pdo >> 10) & 0x3FF);
                            uint16_t i_raw = ((pdo >> 0) & 0x3FF);
                            uint16_t voltage_mv = v_raw * 50;
                            uint16_t current_ma = i_raw * 10;
                        }
                    }
                    
                    pd_monitor.selected_position = 1; // 默认选择第一个电源
                    
                    // 使用第一个PDO作为默认电源信息
                    if (num_obj > 0) {
                        uint32_t first_pdo = obj[0];
                        uint8_t pdo_type = (first_pdo >> 30) & 0x03;
                        
                        if (pdo_type == 0) { // Fixed Supply
                            uint16_t voltage_raw = ((first_pdo >> 10) & 0x3FF);
                            uint16_t current_raw = ((first_pdo >> 0) & 0x3FF);
                            pd_monitor.last_voltage = voltage_raw * 50;
                            pd_monitor.last_current = current_raw * 10;
                            pd_monitor.power_status = STATUS_POWER_TYP;
                        } else if (pdo_type == 3) {
                            // PPS模式：虽然等待Request，但仍要记录第一个PDO用于推测电源模式
                            pd_monitor.power_status = STATUS_POWER_NA;
                            // v1.3修复：PPS模式推测：记录最小电压作为默认
                            uint16_t v_min_raw = ((first_pdo >> 17) & 0x7FF);
                            uint16_t i_max_raw = ((first_pdo >> 7) & 0xFF);
                            pd_monitor.last_voltage = v_min_raw * 100; // 记录最小电压
                            pd_monitor.last_current = i_max_raw * 50;   // 记录最大电流
                        }
                    }
                }
                else if (num_obj > 0 && msg_type == 0x02) {
                    // Request - 从缓存的Source PDO中查找匹配
                    uint32_t request_pdo = obj[0];
                    uint8_t obj_position = (request_pdo >> 28) & 0x0F; // Object position (1-based)
                    
                    if (pd_monitor.source_pdo_count == 0) {
                        // 无缓存PDO，检查是否是PPS Request
                        if (request_pdo & (1U << 27)) {
                            // PPS模式：直接从Request解析
                            uint16_t voltage_raw = ((request_pdo >> 17) & 0x7FF);  // 100mV units
                            uint16_t current_raw = ((request_pdo >> 7) & 0xFF);    // 50mA units
                            
                            pd_monitor.last_voltage = voltage_raw * 100; // 转换为mV
                            pd_monitor.last_current = current_raw * 50;   // 转换为mA
                            pd_monitor.power_status = STATUS_POWER_PPS;
                            pd_monitor.selected_position = obj_position;
                            
                            // v1.3调试：显示即时PPS解析（无缓存）
                            Serial.printf("[LIVE-PPS-NC] 立即更新: %umV %umA pos=%d\n", 
                                          voltage_raw * 100, current_raw * 50, obj_position);
                        }
                    } else if (obj_position > 0 && obj_position <= pd_monitor.source_pdo_count) {
                        // Request在有效范围内，从缓存PDO中查找匹配
                        uint32_t source_pdo = pd_monitor.source_pdos[obj_position - 1];
                        uint8_t source_pdo_type = (source_pdo >> 30) & 0x03;
                        
                        uint16_t voltage = 0;
                        uint16_t current = 0;
                        status_power_t power_status = STATUS_POWER_TYP;
                        
                        if (request_pdo & (1U << 27)) {
                            // PPS模式：直接从Request中解析（最可靠）
                            uint16_t voltage_raw = ((request_pdo >> 17) & 0x7FF);
                            uint16_t current_raw = ((request_pdo >> 7) & 0xFF);
                            voltage = voltage_raw * 100; // 转换为mV
                            current = current_raw * 50;   // 转换为mA
                            power_status = STATUS_POWER_PPS;
                            
                            // v1.3调试：显示即时PPS解析
                            Serial.printf("[LIVE-PPS] 立即更新: %umV %umA pos=%d\n", 
                                          voltage, current, obj_position);
                        } else if (source_pdo_type == 0) {
                            // Fixed模式：从Source PDO中获取
                            uint16_t voltage_raw = ((source_pdo >> 10) & 0x3FF);
                            uint16_t current_raw = ((source_pdo >> 0) & 0x3FF);
                            voltage = voltage_raw * 50;
                            current = current_raw * 10;
                            power_status = STATUS_POWER_TYP;
                        }
                        
                        pd_monitor.last_voltage = voltage;
                        pd_monitor.last_current = current;
                        pd_monitor.power_status = power_status;
                        pd_monitor.selected_position = obj_position;
                        

                    } else {
                        // Request超出PDO范围，这表明设备请求了不存在的电源
                        // 但对于PPS模式，仍然可以从Request中解析电压电流
                        if (request_pdo & (1U << 27)) {
                            // PPS模式即使超出范围也能解析
                            uint16_t voltage_raw = ((request_pdo >> 17) & 0x7FF);
                            uint16_t current_raw = ((request_pdo >> 7) & 0xFF);
                            pd_monitor.last_voltage = voltage_raw * 100; // 转换为mV
                            pd_monitor.last_current = current_raw * 50;   // 转换为mA
                            pd_monitor.power_status = STATUS_POWER_PPS;
                            pd_monitor.selected_position = obj_position;
                            
                            // 注意：这种请求在真实PD协议中会被拒绝
                        }
                        // Fixed模式的无效Request不更新状态
                    }
                }
                else if (msg_type == 0x03) {
                    // PS_RDY - 电源就绪消息（立即更新）
                    if (num_obj > 0 && obj[0] != 0) {
                        uint32_t pdo = obj[0];
                        uint8_t pdo_type = (pdo >> 30) & 0x03;
                        
                        if (pdo_type == 0) { // Fixed Supply
                            uint16_t voltage_raw = ((pdo >> 10) & 0x3FF);
                            uint16_t current_raw = ((pdo >> 0) & 0x3FF);
                            pd_monitor.last_voltage = voltage_raw * 50;
                            pd_monitor.last_current = current_raw * 10;
                            pd_monitor.power_status = STATUS_POWER_TYP;
                            
                            Serial.printf("[LIVE-PSRDY-FIX] 立即更新: %umV %umA\n", 
                                          voltage_raw * 50, current_raw * 10);
                        } else if (pdo_type == 3) { // PPS
                            uint16_t voltage_raw = ((pdo >> 17) & 0x7FF);
                            uint16_t current_raw = ((pdo >> 7) & 0xFF);
                            pd_monitor.last_voltage = voltage_raw * 100;
                            pd_monitor.last_current = current_raw * 50;
                            pd_monitor.power_status = STATUS_POWER_PPS;
                            
                            Serial.printf("[LIVE-PSRDY-PPS] 立即更新: %umV %umA\n", 
                                          voltage_raw * 100, current_raw * 50);
                        }
                    }
                }
            }
            
            if (FUSB302_events & FUSB302_EVENT_GOOD_CRC_SENT) {
                pd_monitor.good_crc_count++;
            }
            
            // v1.3优化：更新监听信息（简化版）
            pd_monitor.last_timestamp = clock_ms();
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// v1.3优化：监听函数实现 - 返回实际数值
///////////////////////////////////////////////////////////////////////////////////////////////////

float PD_UFP_c::get_bridge_voltage(void)
{
    // v1.3修复：Bridge模式：正确处理PPS和Fixed模式的单位转换
    if (bridge_mode_enabled) {
        if (pd_monitor.last_voltage > 0) {
            if (pd_monitor.power_status == STATUS_POWER_PPS) {
                // PPS模式：pd_monitor.last_voltage是以mV为单位的实际值
                float voltage_v = pd_monitor.last_voltage / 1000.0f; // 转换为伏特
                return voltage_v;
            } else if (pd_monitor.power_status == STATUS_POWER_TYP) {
                // Fixed模式：pd_monitor.last_voltage是以mV为单位的实际值
                float voltage_v = pd_monitor.last_voltage / 1000.0f; // 转换为伏特
                return voltage_v;
            }
        }
        return 0.0f;
    }
    
    // 非Bridge模式：使用原有逻辑
    // 将内部电压值转换为实际电压值（V）
    // 内部电压值以50mV为单位（PPS以20mV为单位）
    if (status_power == STATUS_POWER_PPS) {
        // PPS模式：20mV单位
        return ready_voltage * 0.02f; // 转换为伏特
    } else if (status_power == STATUS_POWER_TYP) {
        // 固定电压模式：50mV单位
        return ready_voltage * 0.05f; // 转换为伏特
    }
    return 0.0f; // 无电源时返回0V
}

float PD_UFP_c::get_bridge_current(void)
{
    // v1.3修复：Bridge模式：正确处理PPS和Fixed模式的单位转换
    if (bridge_mode_enabled) {
        if (pd_monitor.last_current > 0) {
            if (pd_monitor.power_status == STATUS_POWER_PPS) {
                // PPS模式：pd_monitor.last_current是以mA为单位的实际值
                float current_a = pd_monitor.last_current / 1000.0f; // 转换为安培
                return current_a;
            } else if (pd_monitor.power_status == STATUS_POWER_TYP) {
                // Fixed模式：pd_monitor.last_current是以mA为单位的实际值
                float current_a = pd_monitor.last_current / 1000.0f; // 转换为安培
                return current_a;
            }
        }
        return 0.0f;
    }
    
    // 非Bridge模式：使用原有逻辑
    // 将内部电流值转换为实际电流值（A）
    // 内部电流值以10mA为单位（PPS以50mA为单位）
    if (status_power == STATUS_POWER_PPS) {
        // PPS模式：50mA单位
        return ready_current * 0.05f; // 转换为安培
    } else if (status_power == STATUS_POWER_TYP) {
        // 固定电流模式：10mA单位
        return ready_current * 0.01f; // 转换为安培
    }
    return 0.0f; // 无电源时返回0A
}

String PD_UFP_c::get_bridge_power_mode(void)
{
    // v1.3修复：Bridge模式：强化PPS模式检测逻辑
    if (bridge_mode_enabled) {
        // 优先检查明确的电源状态
        if (pd_monitor.power_status == STATUS_POWER_PPS) {
            return "PPS";
        } else if (pd_monitor.power_status == STATUS_POWER_TYP) {
            return "FIX";
        } else if (pd_monitor.cc_status) {
            // 设备已连接，但电源状态未确定，立即推测
            
            // 方案1：检查缓存的PDO类型
            if (pd_monitor.source_pdo_count > 0) {
                uint32_t first_pdo = pd_monitor.source_pdos[0];
                uint8_t pdo_type = (first_pdo >> 30) & 0x03;
                
                if (pdo_type == 3) { // PPS类型
                    return "PPS";
                } else if (pdo_type == 0) { // Fixed类型
                    return "FIX";
                }
            }
            
            // 方案2：检查最近的PDO消息（最优先）
            if (pd_monitor.last_msg_obj_count > 0) {
                uint32_t first_obj = pd_monitor.last_msg_obj[0];
                uint8_t msg_type = pd_monitor.last_msg_type;
                
                // 如果最后接收的是Request消息，且包含PPS特征
                if (msg_type == 0x02 && (first_obj & (1U << 27))) {
                    return "PPS";
                }
                
                // 如果最后接收的是Source Capabilities，检查PDO类型
                if (msg_type == 0x01) {
                    uint8_t pdo_type = (first_obj >> 30) & 0x03;
                    if (pdo_type == 3) {
                        return "PPS";
                    } else if (pdo_type == 0) {
                        return "FIX";
                    }
                }
            }
            
            // 方案3：检查电压电流值模式（备用）
            if (pd_monitor.last_voltage > 0 && pd_monitor.last_current > 0) {
                // 检查电流值是否符合PPS模式特征（通常是50mA的倍数）
                if (pd_monitor.last_current % 50 == 0) {
                    return "PPS";
                }
                // 检查电压值是否符合PPS模式特征（通常是100mV的倍数）
                if (pd_monitor.last_voltage % 100 == 0) {
                    return "PPS";
                }
            }
            
            return "PPS?"; // 默认推测为PPS模式
        }
        return "NA";
    }
    
    // 非Bridge模式：使用内部状态
    if (status_power == STATUS_POWER_PPS) {
        return "PPS";
    } else if (status_power == STATUS_POWER_TYP) {
        return "FIX";
    }
    return "NA";
}

uint32_t PD_UFP_c::get_bridge_packet_count(void)
{
    // 返回PD包数量
    return pd_monitor.packet_count;
}

uint8_t PD_UFP_c::get_bridge_src_cap_count(void)
{
    // 返回源能力计数
    return pd_monitor.src_cap_count;
}

uint8_t PD_UFP_c::get_bridge_selected_position(void)
{
    // 返回PD位置
    return pd_monitor.selected_position;
}

uint8_t PD_UFP_c::get_bridge_cc_pin(void)
{
    // 返回CC线状态：0/NULL 1/CC1 2/CC2
    return pd_monitor.cc_pin;
}

uint32_t PD_UFP_c::get_bridge_good_crc_count(void)
{
    // 返回成功CRC计数
    return pd_monitor.good_crc_count;
}

uint32_t PD_UFP_c::get_bridge_reject_count(void)
{
    // 返回拒绝计数
    return pd_monitor.reject_count;
}

bool PD_UFP_c::get_bridge_cc_status(void)
{
    // 返回CC线连接状态
    return pd_monitor.cc_status;
}

void PD_UFP_c::reset_bridge_monitor(void)
{
    // v1.3优化：重置监听数据
    pd_monitor.packet_count = 0;
    pd_monitor.good_crc_count = 0;
    pd_monitor.reject_count = 0;
    pd_monitor.cc_status = false;
    pd_monitor.cc_pin = 0;
    pd_monitor.src_cap_count = 0;
    pd_monitor.selected_position = 0;
    // 保持电压、电流、时间戳不变
}

void PD_UFP_c::set_bridge_log_level(pd_bridge_log_level_t level)
{
    bridge_log_level = level;
}

void PD_UFP_c::force_refresh_bridge_status(void)
{
    // v1.3优化：强制刷新Bridge状态
    if (bridge_mode_enabled && pd_monitor.cc_status && pd_monitor.src_cap_count > 0) {
        // 由于没有缓存机制，这个函数主要用于保持接口兼容性
        // 实际状态已经在run_Bridge()中实时更新
    }
}

String PD_UFP_c::get_bridge_power_info_string(void)
{
    // v1.3优化：获取电源信息的字符串描述
    if (bridge_mode_enabled && pd_monitor.last_voltage > 0) {
        String info = String(pd_monitor.last_voltage / 1000.0f, 2) + "V ";
        info += String(pd_monitor.last_current / 1000.0f, 2) + "A ";
        info += (pd_monitor.power_status == STATUS_POWER_PPS) ? "PPS" : "FIX";
        info += " Pos" + String(pd_monitor.selected_position);
        return info;
    }
    return "No Power";
}

uint32_t PD_UFP_c::get_bridge_max_power(void)
{
    // v1.3优化：获取最大功率（mW）
    if (bridge_mode_enabled && pd_monitor.last_voltage > 0 && pd_monitor.last_current > 0) {
        return (uint32_t)pd_monitor.last_voltage * pd_monitor.last_current / 1000; // mW
    }
    return 0;
}

uint16_t PD_UFP_c::get_bridge_voltage_range_min(void)
{
    // v1.3优化：获取最小电压（mV）
    if (bridge_mode_enabled) {
        return pd_monitor.last_voltage; // 当前电压作为最小值
    }
    return 0;
}

uint16_t PD_UFP_c::get_bridge_voltage_range_max(void)
{
    // v1.3优化：获取最大电压（mV）
    if (bridge_mode_enabled) {
        return pd_monitor.last_voltage; // 当前电压作为最大值
    }
    return 0;
}

uint16_t PD_UFP_c::get_bridge_current_limit(void)
{
    // v1.3优化：获取电流限制（mA）
    if (bridge_mode_enabled) {
        return pd_monitor.last_current; // 当前电流作为限制
    }
    return 0;
}

bool PD_UFP_c::is_bridge_pps_capable(void)
{
    // v1.3优化：检查是否支持PPS
    if (bridge_mode_enabled) {
        return pd_monitor.power_status == STATUS_POWER_PPS;
    }
    return false;
}

uint8_t PD_UFP_c::get_cc_pin()
{
    // 获取CC引脚状态
    if (bridge_mode_enabled) {
        return pd_monitor.cc_pin;
    }
    
    uint8_t cc1 = 0, cc2 = 0;
    FUSB302_get_cc(&FUSB302, &cc1, &cc2);
    
    if (cc1 && cc2 == 0) {
        return 1; // CC1
    } else if (cc2 && cc1 == 0) {
        return 2; // CC2
    }
    return 0; // 无连接
}

bool PD_UFP_c::set_PPS(uint16_t PPS_voltage, uint8_t PPS_current)
{
    // 设置PPS参数
    PPS_voltage_next = PPS_voltage;
    PPS_current_next = PPS_current;
    send_request = 1;
    return true;
}

void PD_UFP_c::set_power_option(enum PD_power_option_t power_option)
{
    // 设置电源选项
    PD_protocol_set_power_option(&protocol, power_option);
}

void PD_UFP_c::clock_prescale_set(uint8_t prescaler)
{
    // 设置时钟分频
    clock_prescaler = prescaler;
}

int PD_UFP_c::status_bridge_log_readline(char *buffer, int maxlen)
{
    // v1.3优化：极简日志输出格式
    if (!bridge_mode_enabled || !buffer || maxlen <= 0 || maxlen > 1024) {
        return 0;
    }
    
    // 确保pd_monitor结构体已初始化
    if (pd_monitor.last_timestamp == 0 && bridge_mode_enabled) {
        pd_monitor.last_timestamp = clock_ms();
        pd_monitor.packet_count = 0;
        pd_monitor.cc_pin = 0;
        pd_monitor.cc_status = false;
    }
    
    int n = 0;
    char time_str[8];
    
    // 按照status_log_readline的格式生成时间戳
    uint16_t time_ms = (uint16_t)(pd_monitor.last_timestamp % 10000);
    snprintf(time_str, sizeof(time_str), "%04u: ", time_ms);
    
    // v1.3优化：统一输出格式，直接包含所有重要信息
    if (pd_monitor.cc_status) {
        // 有连接时的完整信息输出
        float voltage = get_bridge_voltage();
        float current = get_bridge_current();
        uint32_t power_mw = get_bridge_max_power();
        uint8_t pdo_count = pd_monitor.src_cap_count;
        uint8_t selected_pos = pd_monitor.selected_position;
        uint32_t packet_count = pd_monitor.packet_count;
        uint32_t crc_count = pd_monitor.good_crc_count;
        uint8_t cc_pin = pd_monitor.cc_pin;
        String power_mode = get_bridge_power_mode();
        const char* mode_str = power_mode.c_str();
        
        // 主要电源信息行（基础信息）
        n = snprintf(buffer, maxlen, "%s%d.%02dV %d.%02dA %s %uW\n",
                    time_str,
                    (int)voltage, (int)((voltage - (int)voltage) * 100 + 0.5f),
                    (int)current, (int)((current - (int)current) * 100 + 0.5f),
                    mode_str, power_mw / 1000);
        
        // 添加统计信息行（PKT、CRC、CC引脚）
        if (n < maxlen - 50) {
            int stats_len = snprintf(buffer + n, maxlen - n, "%s  CC:%d PKT:%u CRC:%u\n",
                                   time_str, cc_pin, packet_count, crc_count);
            n += stats_len;
        }
        
        // 添加最新PDO消息信息（十六进制数据）
        if (n < maxlen - 100 && pd_monitor.last_msg_obj_count > 0) {
            // 消息头信息
            int msg_len = snprintf(buffer + n, maxlen - n, "%s  MSG:0x%04X TYPE:%d\n",
                                 time_str, pd_monitor.last_msg_header, pd_monitor.last_msg_type);
            n += msg_len;
            
            // 输出PDO对象的十六进制数据（降低空间要求，确保OBJ信息显示）
            for (uint8_t i = 0; i < pd_monitor.last_msg_obj_count && i < 7 && n < maxlen - 20; i++) {
                int obj_len = snprintf(buffer + n, maxlen - n, "%s  OBJ[%d]:0x%08lX\n",
                                     time_str, i, pd_monitor.last_msg_obj[i]);
                n += obj_len;
            }
            
            // 实时解析当前消息的电压电流（立即更新显示）
            if (pd_monitor.last_msg_type == 0x02 && pd_monitor.last_msg_obj_count > 0) {
                // Request消息：直接解析Request的PDO
                uint32_t request_pdo = pd_monitor.last_msg_obj[0];
                
                if (request_pdo & (1U << 27)) {
                    // PPS模式：直接从Request解析
                    uint16_t voltage_raw = ((request_pdo >> 17) & 0x7FF);
                    uint16_t current_raw = ((request_pdo >> 7) & 0xFF);
                    
                    float voltage = voltage_raw * 0.1f;  // 转换为V
                    float current = current_raw * 0.05f; // 转换为A
                    
                    int live_len = snprintf(buffer + n, maxlen - n, "%s  %.1fV %.2fA PPS\n",
                                          time_str, voltage, current);
                    n += live_len;
                } else {
                    // Fixed模式：从Request的Object Position查找
                    uint8_t obj_position = (request_pdo >> 28) & 0x0F;
                    
                    int live_len = snprintf(buffer + n, maxlen - n, "%s  Fixed PDO[%d] pos[%d]\n",
                                          time_str, pdo_count, selected_pos);
                    n += live_len;
                }
            } else if (pd_monitor.last_msg_type == 0x03 && pd_monitor.last_msg_obj_count > 0) {
                // PS_RDY消息：显示最终电源状态
                uint32_t pdo = pd_monitor.last_msg_obj[0];
                uint8_t pdo_type = (pdo >> 30) & 0x03;
                
                if (pdo_type == 0) {
                    uint16_t voltage_raw = ((pdo >> 10) & 0x3FF);
                    uint16_t current_raw = ((pdo >> 0) & 0x3FF);
                    float voltage = voltage_raw * 0.05f;
                    float current = current_raw * 0.01f;
                    
                    int ready_len = snprintf(buffer + n, maxlen - n, "%s  READY: %.1fV %.2fA Fixed\n",
                                           time_str, voltage, current);
                    n += ready_len;
                } else if (pdo_type == 3) {
                    uint16_t voltage_raw = ((pdo >> 17) & 0x7FF);
                    uint16_t current_raw = ((pdo >> 7) & 0xFF);
                    float voltage = voltage_raw * 0.1f;
                    float current = current_raw * 0.05f;
                    
                    int ready_len = snprintf(buffer + n, maxlen - n, "%s  READY: %.1fV %.2fA PPS\n",
                                           time_str, voltage, current);
                    n += ready_len;
                }
            }
        }
    } else {
        // 无连接时的输出
        n = snprintf(buffer, maxlen, "%sNo PD device connected\n", time_str);
    }
    
    return n;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PD_UFP_Log_c
///////////////////////////////////////////////////////////////////////////////////////////////////

PD_UFP_Log_c::PD_UFP_Log_c(pd_log_level_t log_level, pd_bridge_log_level_t bridge_log_level)
{
    this->status_log_level = log_level;
    this->bridge_log_level = bridge_log_level;
    status_log_read = 0;
    status_log_write = 0;
    status_log_obj_read = 0;
    status_log_obj_write = 0;
    status_log_counter = 0;
}

void PD_UFP_Log_c::print_status(HardwareSerial & serial)
{
    // 实现日志打印功能
    char buffer[128];
    int len = status_log_readline(buffer, sizeof(buffer));
    if (len > 0) {
        serial.print(buffer);
    }
}

int PD_UFP_Log_c::status_log_readline(char * buffer, int maxlen)
{
    int len = 0;
    
    if (((status_log_write - status_log_read) & STATUS_LOG_MASK) == 0) {
        return 0;
    }
    
    status_log_t * log = &status_log[status_log_read & STATUS_LOG_MASK];
    status_log_read++;
    
    switch (log->status) {
    case STATUS_LOG_MSG_TX:
        len = status_log_readline_msg(buffer, maxlen, log);
        break;
    case STATUS_LOG_MSG_RX:
        len = status_log_readline_msg(buffer, maxlen, log);
        break;
    case STATUS_LOG_SRC_CAP:
        len = status_log_readline_src_cap(buffer, maxlen);
        break;
    default:
        len = SNPRINTF(buffer, maxlen, MY_PSTR("%s: %d\n"), status_log_time, log->status);
        break;
    }
    
    return len;
}

int PD_UFP_Log_c::status_log_readline_msg(char * buffer, int maxlen, status_log_t * log)
{
    int len = 0;
    int i;
    
    switch (log->status) {
    case STATUS_LOG_MSG_TX:
        len = SNPRINTF(buffer, maxlen, MY_PSTR("Tx: "));
        break;
    case STATUS_LOG_MSG_RX:
        len = SNPRINTF(buffer, maxlen, MY_PSTR("Rx: "));
        break;
    default:
        len = SNPRINTF(buffer, maxlen, MY_PSTR("??: "));
        break;
    }
    
    for (i = 0; i < log->obj_count && len < maxlen - 1; i++) {
        uint32_t obj = status_log_obj[status_log_obj_read & STATUS_LOG_MASK];
        status_log_obj_read++;
        len += SNPRINTF(buffer + len, maxlen - len, MY_PSTR("%08lX "), obj);
    }
    
    if (len < maxlen) {
        buffer[len++] = '\n';
    }
    
    return len;
}

int PD_UFP_Log_c::status_log_readline_src_cap(char * buffer, int maxlen)
{
    int len = SNPRINTF(buffer, maxlen, MY_PSTR("SRC_CAP\n"));
    
    if (len < maxlen) {
        int i;
        for (i = 0; i < get_src_cap_count() && len < maxlen - 1; i++) {
            uint32_t obj = protocol.power_data_obj[i];
            len += SNPRINTF(buffer + len, maxlen - len, MY_PSTR("%08lX "), obj);
        }
        if (len < maxlen) {
            buffer[len++] = '\n';
        }
    }
    
    return len;
}

uint8_t PD_UFP_Log_c::status_log_obj_add(uint16_t header, uint32_t * obj)
{
    uint8_t obj_count = (header >> 12) & 0x07;
    
    if (obj_count > 7) {
        obj_count = 7;
    }
    
    for (uint8_t i = 0; i < obj_count; i++) {
        status_log_obj[status_log_obj_write & STATUS_LOG_MASK] = obj[i];
        status_log_obj_write++;
    }
    
    return obj_count;
}

void PD_UFP_Log_c::status_log_event(uint8_t status, uint32_t * obj)
{
    if (((status_log_write - status_log_read) & STATUS_LOG_MASK) >= STATUS_LOG_MASK) {
        return;
    }
    status_log_t * log = &status_log[status_log_write & STATUS_LOG_MASK];
    switch (status) {
    case STATUS_LOG_MSG_TX:
        log->msg_header = PD_protocol_get_tx_msg_header(&protocol);
        log->obj_count = status_log_obj_add(log->msg_header, obj);
        break;
    case STATUS_LOG_MSG_RX:
        log->msg_header = PD_protocol_get_rx_msg_header(&protocol);
        log->obj_count = status_log_obj_add(log->msg_header, obj);
        break;
    default:
        break;
    }
    log->status = status;
    log->time = clock_ms();
    status_log_write++;
    // v1.3优化：更新监听信息
    update_monitor_info();
}

void PD_UFP_Log_c::init_Bridge(uint8_t int_pin)
{
    PD_UFP_c::init_Bridge(int_pin);
}

void PD_UFP_Log_c::run_Bridge(void)
{
    PD_UFP_c::run_Bridge();
}

int PD_UFP_Log_c::status_bridge_log_readline(char *buffer, int maxlen)
{
    if (!buffer || maxlen <= 0) return 0;
    
    int len = 0;
    
    // 首先返回父类的完整状态信息（包括PKT、CRC、CC等）
    len = PD_UFP_c::status_bridge_log_readline(buffer, maxlen);
    
    // v1.3优化：子类只添加额外的调试信息，避免重复
    if (len < maxlen - 1 && bridge_mode_enabled) {
        int remaining = maxlen - len - 1; // 留一个字符给终止符
        if (remaining > 0) {
            buffer[len] = '\n'; // 添加换行符分隔
            len++;
            
            // 安全检查bridge_status_buffer
            const char* status_str = (bridge_status_buffer[0] != '\0') ? bridge_status_buffer : "Unknown";
            
            if (bridge_log_level >= (pd_bridge_log_level_t)1) { // 1 = DETAILED
                uint16_t retry_count = (get_src_cap_retry_count > 10) ? 0 : get_src_cap_retry_count;
                uint8_t pdo_cache_position = pd_monitor.selected_position; // 缓存数组中当前选择的PDO位置
                uint8_t pdo_cache_total = pd_monitor.source_pdo_count; // 实际PDO总数
                uint8_t pdo_cached_count = (pdo_cache_total > 7) ? 7 : pdo_cache_total; // 实际缓存的PDO数量
                
                if (pdo_cache_total > 7) {
                    int log_len = snprintf(buffer + len, remaining, 
                        "Debug: %s Retry:%d Init:%s List:%d/%d (Cached:%d)",
                        status_str, retry_count,
                        status_initialized ? "OK" : "ERROR", pdo_cache_position, pdo_cache_total, pdo_cached_count);
                    if (log_len > 0 && log_len < remaining) {
                        len += log_len;
                    }
                } else {
                    int log_len = snprintf(buffer + len, remaining, 
                        "Debug: %s Retry:%d Init:%s List:%d/%d",
                        status_str, retry_count,
                        status_initialized ? "OK" : "ERROR", pdo_cache_position, pdo_cache_total);
                    if (log_len > 0 && log_len < remaining) {
                        len += log_len;
                    }
                }
            } else {
                // 基础模式：只显示基本状态信息
                int log_len = snprintf(buffer + len, remaining, "Bridge: %s", status_str);
                if (log_len > 0 && log_len < remaining) {
                    len += log_len;
                }
            }
        }
    }
    
    return len;
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
