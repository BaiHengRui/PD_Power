/**
 * PD_UFP.cpp
 *
 *      Author: Ryan Ma
 *      Edited: Kai Liebich
 *      v1.2: Bridge模式完整修复版本
 *
 * Minimalist USB PD Ardunio Library for PD Micro board
 * Only support UFP(device) sink only functionality
 * Requires FUSB302_UFP.h, PD_UFP_Protocol.h and Standard Arduino Library
 *
 * Support PD3.0 PPS
 * 
 * - 修复Bridge模式循环握手问题
 * - 修复Request PDO解析逻辑，正确从Object position查找Source PDO
 * - 修复PPS模式电压电流解析，直接从Request获取当前值
 * - 优化消息处理和状态更新逻辑
 * - 提供详细的PD消息监控和清晰的日志输出
 * - 强制禁用所有PD握手协议调用
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
    bridge_log_index(0)
{
    memset(&FUSB302, 0, sizeof(FUSB302_dev_t));
    memset(&protocol, 0, sizeof(PD_protocol_t));
    // v1.2修复：初始化监听数据
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
    // v1.2修复：Bridge模式下不调用run()函数，只使用run_Bridge()
    if (bridge_mode_enabled) {
        Serial.println("[RUN] Bridge模式：使用run_Bridge()而不是run()");
        return;
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

void PD_UFP_c::handle_protocol_event(PD_protocol_event_t events)
{
    // v1.2修复：Bridge模式下完全禁用handle_protocol_event
    if (bridge_mode_enabled) {
        Serial.println("[PROTOCOL] Bridge模式：禁用协议事件处理");
        return;
    }
    
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
            FUSB302_set_vbus_sense(&FUSB302, 0);
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
        // v1.2修复：Bridge模式下不调用PD_protocol_reset
        if (!bridge_mode_enabled) {
            PD_protocol_reset(&protocol);
        } else {
            Serial.println("[BRIDGE] Bridge模式：断开连接，不重置PD协议");
        }
        
        // Bridge模式下的特殊处理
        if (bridge_mode_enabled) {
            pd_monitor.cc_status = false;
            pd_monitor.cc_pin = 0;
        }
        return;
    }
    if (events & FUSB302_EVENT_ATTACHED) {
        uint8_t cc1 = 0, cc2 = 0, cc = 0;
        FUSB302_get_cc(&FUSB302, &cc1, &cc2);
        
        // v1.2修复：Bridge模式下不调用PD_protocol_reset
        if (!bridge_mode_enabled) {
            PD_protocol_reset(&protocol);
        } else {
            Serial.println("[BRIDGE] Bridge模式：连接检测，不重置PD协议");
        }
        
        if (cc1 && cc2 == 0) {
            cc = cc1;
        } else if (cc2 && cc1 == 0) {
            cc = cc2;
        }
        /* TODO: handle no cc detected error */
        if (cc > 1) {
            if (!bridge_mode_enabled) {
                wait_src_cap = 1;
            } else {
                Serial.println("[BRIDGE] Bridge模式：不设置wait_src_cap");
            }
        } else {
            if (!bridge_mode_enabled) {
                set_default_power();
            } else {
                Serial.println("[BRIDGE] Bridge模式：不调用set_default_power");
            }
        }
        
        // Bridge模式下的CC状态更新
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
        
        // v1.2修复：Bridge模式下的数据包计数
        if (bridge_mode_enabled) {
            pd_monitor.packet_count++;
        }
        
        // v1.2修复：Bridge模式下完全不调用任何PD协议函数
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
            Serial.println("[BRIDGE] Bridge模式：不处理PD协议，只记录消息");
            status_log_event(STATUS_LOG_MSG_RX, obj);
        }
    }
    if (events & FUSB302_EVENT_GOOD_CRC_SENT) {
        // v1.2修复：Bridge模式下的Good CRC计数
        if (bridge_mode_enabled) {
            pd_monitor.good_crc_count++;
            Serial.println("[BRIDGE] Bridge模式：不发送PD响应，只统计Good CRC");
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
    // v1.2修复：Bridge模式下完全禁用timer功能
    if (bridge_mode_enabled) {
        Serial.println("[TIMER] Bridge模式：禁用timer功能");
        return false;
    }
    
    uint16_t t = clock_ms();
    if (wait_src_cap && t - time_wait_src_cap > t_TypeCSinkWaitCap) {
        time_wait_src_cap = t;
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
    if (wait_ps_rdy) {
        if (t - time_wait_ps_rdy > t_RequestToPSReady) {
            wait_ps_rdy = 0;
            set_default_power();
        }
    } else if (send_request || (status_power == STATUS_POWER_PPS && t - time_PPS_request > t_PPSRequest)) {
        wait_ps_rdy = 1;
        send_request = 0;
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
    // v1.2修复：Bridge模式下不调用status_power_ready，避免协议状态更新
    if (bridge_mode_enabled) {
        Serial.println("[STATUS] Bridge模式：不调用status_power_ready，只更新监听数据");
        
        // 只更新Bridge监听数据，不更新协议状态
        if (status == STATUS_POWER_PPS) {
            // PPS模式：ready_voltage是20mV单位，ready_current是50mA单位
            pd_monitor.last_voltage = voltage * 20;  // 20mV单位转换为mV
            pd_monitor.last_current = current * 50;  // 50mA单位转换为mA
        } else {
            // Fixed/Variable/Battery模式：ready_voltage是50mV单位，ready_current是10mA单位
            pd_monitor.last_voltage = voltage * 50;  // 50mV单位转换为mV
            pd_monitor.last_current = current * 10;  // 10mA单位转换为mA
        }
        
        pd_monitor.power_status = status;
        pd_monitor.last_timestamp = clock_ms();
        
        Serial.printf("[STATUS] Bridge监听数据更新: status=%d, V_raw=%u, I_raw=%u, V_mV=%umV, I_mA=%umA\n",
                      status, voltage, current, pd_monitor.last_voltage, pd_monitor.last_current);
        return;
    }
    
    // 非Bridge模式：正常协议处理
    ready_voltage = voltage;
    ready_current = current;
    status_power = status;
    
    // 非Bridge模式：直接更新监听数据
    pd_monitor.last_voltage = voltage;
    pd_monitor.last_current = current;
    pd_monitor.power_status = status;
    pd_monitor.last_timestamp = clock_ms();
    
    Serial.printf("[STATUS] 非Bridge模式状态更新: status=%d, V=%umV, I=%umA\n",
                  status, voltage, current);
}

uint8_t PD_UFP_c::clock_prescaler = 1;

void PD_UFP_c::delay_ms(uint16_t ms)
{
    delay(ms / clock_prescaler);
}

uint16_t PD_UFP_c::clock_ms(void)
{
    return (uint16_t)millis() * clock_prescaler;
}

void PD_UFP_c::update_monitor_info(void)
{
    // 更新时间戳
    pd_monitor.last_timestamp = clock_ms();
    
    if (bridge_mode_enabled) {
        // v1.2修复：Bridge模式：只更新时间戳，不访问协议状态
        // 所有状态更新都在run_Bridge()中的事件处理里完成
        
        Serial.println("[UPDATE] Bridge模式：纯监听状态更新，不涉及PD协议");
    } else {
        // 非Bridge模式：正常更新所有信息
        Serial.printf("[UPDATE] 非Bridge模式调用get_ps_status(): %d\n", get_ps_status());
        pd_monitor.last_voltage = get_voltage();
        pd_monitor.last_current = get_current();
        pd_monitor.power_status = get_ps_status();
        pd_monitor.cc_pin = get_cc_pin();
        pd_monitor.cc_status = (pd_monitor.cc_pin != 0);
        pd_monitor.src_cap_count = get_src_cap_count();
        pd_monitor.selected_position = get_selected_position();
    }
    
    // v1.2修复：Bridge模式下不调用任何PD协议函数
    if (bridge_mode_enabled) {
        // Serial.println("[UPDATE] Bridge模式：纯监听状态更新，不涉及PD协议");
    }
}

void PD_UFP_c::reset_monitor_info(void)
{
    memset(&pd_monitor, 0, sizeof(pd_monitor_t));
    pd_monitor.last_timestamp = clock_ms();
    
    // v1.2修复：重置Source PDO缓存
    pd_monitor.source_pdo_count = 0;
    memset(pd_monitor.source_pdos, 0, sizeof(pd_monitor.source_pdos));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// v1.2修复：Bridge功能方法实现
///////////////////////////////////////////////////////////////////////////////////////////////////

void PD_UFP_c::init_Bridge(uint8_t int_pin)
{
    this->int_pin = int_pin;
    bridge_mode_enabled = true;
    bridge_log_level = PD_BRIDGE_LOG_LEVEL_DETAILED; // 默认使用详细模式
    
    // Initialize FUSB302 for bridge mode
    pinMode(int_pin, INPUT_PULLUP); // Set FUSB302 int pin input and pull up
    FUSB302.i2c_address = 0x22;
    FUSB302.i2c_read = FUSB302_i2c_read;
    FUSB302.i2c_write = FUSB302_i2c_write;
    FUSB302.delay_ms = FUSB302_delay_ms;
    
    if (FUSB302_init(&FUSB302) == FUSB302_SUCCESS && FUSB302_get_ID(&FUSB302, 0, 0) == FUSB302_SUCCESS) {
        status_initialized = 1;
    }
    
    // v1.2修复：Bridge模式完全不初始化PD协议引擎
    // 禁用所有PD协议处理
    Serial.println("[BRIDGE] Bridge模式：禁用PD协议引擎，纯监听模式");
    
    // v1.2修复：强制清除所有可能触发握手的标志
    wait_src_cap = 0;
    wait_ps_rdy = 0;
    send_request = 0;
    get_src_cap_retry_count = 0;
    
    Serial.println("[BRIDGE] 握手标志已清除，完全禁用PD协议");
    
    // Clear bridge log buffer
    memset(bridge_log_buffer, 0, sizeof(bridge_log_buffer));
    bridge_log_index = 0;
    
    // v1.2修复：初始化监听数据
    memset(&pd_monitor, 0, sizeof(pd_monitor_t));
    pd_monitor.last_timestamp = clock_ms();
    
    // v1.2修复：初始化Source PDO缓存
    pd_monitor.source_pdo_count = 0;
    memset(pd_monitor.source_pdos, 0, sizeof(pd_monitor.source_pdos));
    
    // 设置默认的监听状态
    pd_monitor.power_status = STATUS_POWER_NA;  // 未知电源状态
    pd_monitor.selected_position = 1;  // 默认选择第一个电源（位置1）
    
    // v1.2修复：初始化内部状态
    status_power = STATUS_POWER_NA;  // 确保内部状态正确初始化
    ready_voltage = 0;
    ready_current = 0;
    
    Serial.println("[BRIDGE] Bridge模式初始化完成");
    Serial.printf("[BRIDGE] 初始化状态: status_power=%d, ready_voltage=%u, ready_current=%u\n",
                  status_power, ready_voltage, ready_current);
}

void PD_UFP_c::run_Bridge(void)
{
    if (!bridge_mode_enabled) return;
    
    // v1.2修复：彻底禁用所有PD握手逻辑
    // 纯监听模式：只记录PD消息，不进行任何协议处理
    
    // v1.2修复：不调用timer()函数，避免触发PD协议重试
    // 只在有中断时才处理，且不上报给PD协议层
    static bool bridge_mode_confirmed = false;
    if (!bridge_mode_confirmed) {
        Serial.println("[BRIDGE] === Bridge模式启动：完全禁用PD握手 ===");
        bridge_mode_confirmed = true;
    }
    
    if (digitalRead(int_pin) == 0) {
        FUSB302_event_t FUSB302_events = 0;
        for (uint8_t i = 0; i < 3 && FUSB302_alert(&FUSB302, &FUSB302_events) != FUSB302_SUCCESS; i++) {}
        
        if (FUSB302_events) {
            Serial.printf("[BRIDGE] 纯监听模式事件: 0x%02X (只记录，不处理)\n", FUSB302_events);
            
            // v1.2修复：只处理attach/detach事件用于状态监控
            // 不调用任何PD协议函数，不发送任何响应
            
            if (FUSB302_events & FUSB302_EVENT_DETACHED) {
                // v1.2修复：纯监听模式：设备断开连接，只更新监听状态
                Serial.println("[BRIDGE] 设备断开连接 (纯监听)");
                
                // 只更新监听状态，不调用任何PD协议函数
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
                
                // v1.2修复：重置Source PDO缓存
                pd_monitor.source_pdo_count = 0;
                memset(pd_monitor.source_pdos, 0, sizeof(pd_monitor.source_pdos));
                
                // v1.2修复：不调用status_power_ready，避免触发协议处理
                Serial.println("[BRIDGE] 断开状态已更新，不进行PD握手");
            }
            
            if (FUSB302_events & FUSB302_EVENT_ATTACHED) {
                // v1.2修复：纯监听模式：设备连接，只更新监听状态
                Serial.println("[BRIDGE] 设备连接 (纯监听模式)");
                uint8_t cc1 = 0, cc2 = 0, cc = 0;
                FUSB302_get_cc(&FUSB302, &cc1, &cc2);
                
                if (cc1 && cc2 == 0) {
                    cc = cc1;
                } else if (cc2 && cc1 == 0) {
                    cc = cc2;
                }
                
                // 只更新监听状态，不触发任何PD握手
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
                
                // v1.2修复：重置Source PDO缓存
                pd_monitor.source_pdo_count = 0;
                memset(pd_monitor.source_pdos, 0, sizeof(pd_monitor.source_pdos));
                
                Serial.println("[BRIDGE] 连接状态已更新，纯监听模式启动");
            }
            
            if (FUSB302_EVENT_RX_SOP & FUSB302_events) {
                // v1.2修复：Bridge模式：详细解析PD消息，完全禁用PD协议
                uint16_t header;
                uint32_t obj[7];
                FUSB302_get_message(&FUSB302, &header, obj);
                
                pd_monitor.packet_count++;
                
                // 详细解析消息头信息
                uint16_t msg_header = header;
                uint8_t num_obj = (msg_header >> 12) & 0x07;
                uint8_t msg_type = (msg_header >> 0) & 0x1F;
                
                // v1.2修复：恢复详细清晰的消息日志
                Serial.printf("[PD_RX] header=0x%04X, objects=%d, type=%d (", 
                              header, num_obj, msg_type);
                
                // 消息类型解析
                switch(msg_type) {
                    case 0x01: Serial.print("Source_Capabilities"); break;
                    case 0x02: Serial.print("Request"); break;
                    case 0x03: Serial.print("PS_RDY"); break;
                    case 0x04: Serial.print("Get_Source_Cap"); break;
                    case 0x05: Serial.print("Get_Sink_Cap"); break;
                    case 0x06: Serial.print("DR_Swap"); break;
                    case 0x07: Serial.print("PR_Swap"); break;
                    case 0x08: Serial.print("VCONN_Swap"); break;
                    case 0x09: Serial.print("Wait"); break;
                    case 0x0A: Serial.print("Soft_Reset"); break;
                    case 0x0B: Serial.print("Data_Reset"); break;
                    case 0x0C: Serial.print("Data_Reset_Complete"); break;
                    case 0x0D: Serial.print("Not_Supported"); break;
                    case 0x0E: Serial.print("Get_Source_Status"); break;
                    case 0x0F: Serial.print("Get_PPS_Status"); break;
                    case 0x10: Serial.print("Get_Sink_Status"); break;
                    case 0x11: Serial.print("Get_Source_Cap_Extended"); break;
                    case 0x12: Serial.print("Get_Battery_Status"); break;
                    case 0x13: Serial.print("Get_Battery_Cap"); break;
                    case 0x14: Serial.print("Get_Manufacturer_Info"); break;
                    case 0x15: Serial.print("Security_Response"); break;
                    case 0x16: Serial.print("Firmware_Update_Request"); break;
                    case 0x17: Serial.print("Firmware_Update_Response"); break;
                    default: Serial.print("Unknown"); break;
                }
                Serial.println(")");
                
                // 详细输出PDO数据
                for (uint8_t i = 0; i < num_obj && i < 7; i++) {
                    Serial.printf("[PD_RX]   PDO[%d]=0x%08lX\n", i, obj[i]);
                }
                
                // v1.2修复：Bridge模式快速处理Source Capabilities
                if (num_obj > 0 && msg_type == 0x01) {
                    Serial.println("[BRIDGE] Source Capabilities - 开始解析");
                    
                    // 缓存所有Source PDO供后续Request解析
                    pd_monitor.src_cap_count = num_obj;
                    pd_monitor.source_pdo_count = num_obj;
                    
                    for (uint8_t i = 0; i < num_obj && i < 7; i++) {
                        pd_monitor.source_pdos[i] = obj[i];
                        
                        // 快速解析PDO信息
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
                                    Serial.printf("[BRIDGE]   PDO[%d]: Fixed %umV %umA\n", i, voltage_mv, current_ma);
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
                                    Serial.printf("[BRIDGE]   PDO[%d]: PPS范围 %umV-%umV, 最大电流%umA\n", 
                                                  i, v_min_mv, v_max_mv, i_max_ma);
                                    // v1.2修复：注意：PPS模式下不从Source Capabilities更新当前状态
                                    // 当前状态从Request消息中获取
                                }
                                break;
                            default:
                                Serial.printf("[BRIDGE]   PDO[%d]: Type %d\n", i, pdo_type);
                                break;
                        }
                    }
                    
                    Serial.printf("[BRIDGE] Source Capabilities: %d个PDO已缓存\n", pd_monitor.source_pdo_count);
                    pd_monitor.selected_position = 1; // 默认选择第一个电源
                    
                    // v1.2修复：使用第一个PDO作为默认电源信息（仅限Fixed模式）
                    if (num_obj > 0) {
                        uint32_t first_pdo = obj[0];
                        uint8_t pdo_type = (first_pdo >> 30) & 0x03;
                        
                        if (pdo_type == 0) { // Fixed Supply
                            uint16_t voltage_raw = ((first_pdo >> 10) & 0x3FF);
                            uint16_t current_raw = ((first_pdo >> 0) & 0x3FF);
                            pd_monitor.last_voltage = voltage_raw * 50;
                            pd_monitor.last_current = current_raw * 10;
                            pd_monitor.power_status = STATUS_POWER_TYP;
                            Serial.printf("[BRIDGE] 默认电源: %umV %umA\n", pd_monitor.last_voltage, pd_monitor.last_current);
                        } else if (pdo_type == 3) {
                            // v1.2修复：PPS模式：不设置默认电源，等待Request消息
                            Serial.println("[BRIDGE] PPS模式：不设置默认电源，等待Request");
                            pd_monitor.power_status = STATUS_POWER_NA;
                        }
                    }
                }
                
                // v1.2修复：Bridge模式快速处理Request消息
                if (num_obj > 0 && msg_type == 0x02) {
                    Serial.println("[BRIDGE] Request - 开始解析");
                    
                    uint32_t request_pdo = obj[0];
                    uint8_t obj_position = (request_pdo >> 28) & 0x0F; // Object position (1-based)
                    
                    Serial.printf("[BRIDGE] Request: PDO=0x%08lX, Position=%u\n", 
                                  request_pdo, obj_position);
                    
                    // v1.2修复：对于PPS模式，直接从Request中解析当前电压电流
                    // 对于Fixed模式，从Source PDO查找
                    if (pd_monitor.source_pdo_count == 0) {
                        Serial.println("[BRIDGE] 警告: 无缓存PDO，跳过Request解析");
                    } else if (obj_position > 0 && obj_position <= pd_monitor.source_pdo_count) {
                        uint32_t source_pdo = pd_monitor.source_pdos[obj_position - 1];
                        uint8_t source_pdo_type = (source_pdo >> 30) & 0x03;
                        
                        Serial.printf("[BRIDGE] 位置%u -> Source PDO=0x%08lX, type=%d\n", 
                                      obj_position, source_pdo, source_pdo_type);
                        
                        uint16_t voltage = 0;
                        uint16_t current = 0;
                        status_power_t power_status = STATUS_POWER_TYP;
                        
                        if (source_pdo_type == 3) {
                            // v1.2修复：PPS模式：直接从Request中解析当前电压电流
                            // PPS Request格式：
                            // Bits 17-27: Output Voltage (100mV units)
                            // Bits 7-15: Operating Current (50mA units)
                            uint16_t voltage_raw = ((request_pdo >> 17) & 0x7FF);  // 100mV units
                            uint16_t current_raw = ((request_pdo >> 7) & 0xFF);    // 50mA units
                            
                            voltage = voltage_raw * 100; // 转换为mV
                            current = current_raw * 50;   // 转换为mA
                            power_status = STATUS_POWER_PPS;
                            
                            Serial.printf("[BRIDGE] PPS Request解析: %umV %umA (原始: %u*100mV, %u*50mA)\n", 
                                          voltage, current, voltage_raw, current_raw);
                        } else {
                            // Fixed模式：从Source PDO查找
                            switch (source_pdo_type) {
                                case 0: // Fixed Supply
                                    {
                                        uint16_t voltage_raw = ((source_pdo >> 10) & 0x3FF);
                                        uint16_t current_raw = ((source_pdo >> 0) & 0x3FF);
                                        voltage = voltage_raw * 50;
                                        current = current_raw * 10;
                                        power_status = STATUS_POWER_TYP;
                                        Serial.printf("[BRIDGE] Fixed: %umV %umA\n", voltage, current);
                                    }
                                    break;
                                default:
                                    Serial.printf("[BRIDGE] 未支持类型: %d\n", source_pdo_type);
                                    break;
                            }
                        }
                        
                        if (voltage > 0) {
                            // 立即更新Bridge状态
                            pd_monitor.last_voltage = voltage;
                            pd_monitor.last_current = current;
                            pd_monitor.power_status = power_status;
                            pd_monitor.selected_position = obj_position;
                            
                            Serial.printf("[BRIDGE] 状态更新: %.3fV %.3fA [%s] 位置%u\n",
                                          voltage/1000.0f, current/1000.0f,
                                          power_status == STATUS_POWER_PPS ? "PPS" : "FIX",
                                          obj_position);
                            
                            // v1.2修复：不调用status_power_ready，避免协议处理
                        }
                    } else {
                        Serial.printf("[BRIDGE] 错误: 位置%u超出范围 (1-%u)\n", 
                                      obj_position, pd_monitor.source_pdo_count);
                    }
                }
                
                // v1.2修复：Bridge模式快速处理PS_RDY消息
                if (msg_type == 0x03) { // PS_RDY消息类型为0x03
                    Serial.println("[BRIDGE] PS_RDY - 电源就绪");
                    Serial.printf("[BRIDGE] PS_RDY: objects=%d\n", num_obj);
                    
                    if (num_obj > 0 && obj[0] != 0) {
                        // 有对象数据，解析PDO
                        uint32_t pdo = obj[0];
                        uint8_t pdo_type = (pdo >> 30) & 0x03;
                        
                        Serial.printf("[BRIDGE] PS_RDY PDO: 0x%08lX, type=%d\n", pdo, pdo_type);
                        
                        if (pdo_type == 0) { // Fixed Supply
                            uint16_t voltage_raw = ((pdo >> 10) & 0x3FF);
                            uint16_t current_raw = ((pdo >> 0) & 0x3FF);
                            uint16_t voltage = voltage_raw * 50;
                            uint16_t current = current_raw * 10;
                            
                            pd_monitor.last_voltage = voltage;
                            pd_monitor.last_current = current;
                            pd_monitor.power_status = STATUS_POWER_TYP;
                            
                            Serial.printf("[BRIDGE] PS_RDY Fixed: %.3fV %.3fA\n", 
                                          voltage/1000.0f, current/1000.0f);
                        } else if (pdo_type == 3) { // PPS
                            uint16_t voltage_raw = ((pdo >> 17) & 0x7FF);
                            uint16_t current_raw = ((pdo >> 7) & 0xFF);
                            uint16_t voltage = voltage_raw * 100;
                            uint16_t current = current_raw * 50;
                            
                            pd_monitor.last_voltage = voltage;
                            pd_monitor.last_current = current;
                            pd_monitor.power_status = STATUS_POWER_PPS;
                            
                            Serial.printf("[BRIDGE] PS_RDY PPS: %.3fV %.3fA\n", 
                                          voltage/1000.0f, current/1000.0f);
                        }
                    } else {
                        // 无对象数据，确认当前状态
                        Serial.println("[BRIDGE] PS_RDY确认当前状态");
                        if (pd_monitor.last_voltage > 0) {
                            Serial.printf("[BRIDGE] 确认: %.3fV %.3fA [%s]\n",
                                          pd_monitor.last_voltage/1000.0f,
                                          pd_monitor.last_current/1000.0f,
                                          pd_monitor.power_status == STATUS_POWER_PPS ? "PPS" : "FIX");
                        }
                    }
                }
                
                // v1.2修复：Bridge模式完全禁用所有PD协议调用
                // 不调用PD_protocol_handle_msg
                // 不调用handle_protocol_event
                // 不调用FUSB302_tx_sop
                // 不调用PD_protocol_respond
                
                Serial.println("[MONITOR] 消息已记录，Bridge模式不参与PD协议");
            }
            
            if (FUSB302_events & FUSB302_EVENT_GOOD_CRC_SENT) {
                // v1.2修复：纯监听模式：Good CRC只统计，完全不发送响应
                pd_monitor.good_crc_count++;
                
                Serial.println("[MONITOR] Good CRC已统计，Bridge模式不发送响应");
                // v1.2修复：不调用PD_protocol_respond
                // v1.2修复：不调用FUSB302_tx_sop
            }
        }
        
        // 更新监听信息
        update_monitor_info();
    }
}

int PD_UFP_c::status_bridge_log_readline(char *buffer, int maxlen)
{
    // v1.2修复：增强的安全检查
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
    
    // 根据日志级别决定输出格式
    if (bridge_log_level == PD_BRIDGE_LOG_LEVEL_BASIC) {
        // 标准模式：只输出电压电流和PDO信息
        float voltage = get_bridge_voltage();
        float current = get_bridge_current();
        uint8_t pdo_count = pd_monitor.src_cap_count;
        uint8_t selected_pos = pd_monitor.selected_position;
        
        if (pd_monitor.cc_status && voltage > 0) {
            n = snprintf(buffer, maxlen, "%s%d.%02dV %d.%02dA pos[%d] PDO[%d]\n",
                        time_str,
                        (int)voltage, (int)((voltage - (int)voltage) * 100 + 0.5f),
                        (int)current, (int)((current - (int)current) * 100 + 0.5f),
                        selected_pos, pdo_count);
        } else {
            n = snprintf(buffer, maxlen, "%sNo PD connection\n", time_str);
        }
    } else {
        // 详细模式：输出所有信息
        String power_mode = get_bridge_power_mode();
        const char* mode_str = power_mode.c_str();
        
        if (pd_monitor.cc_status) {
            // 有连接时的详细输出
            float voltage = get_bridge_voltage();
            float current = get_bridge_current();
            uint32_t power_mw = get_bridge_max_power();
            uint8_t pdo_count = pd_monitor.src_cap_count;
            uint8_t selected_pos = pd_monitor.selected_position;
            uint32_t packet_count = pd_monitor.packet_count;
            uint32_t crc_count = pd_monitor.good_crc_count;
            uint8_t cc_pin = pd_monitor.cc_pin;
            
            // 主要电源信息行
            n = snprintf(buffer, maxlen, "%s%d.%02dV %d.%02dA %s PDO[%d] pos[%d] %uW\n",
                        time_str,
                        (int)voltage, (int)((voltage - (int)voltage) * 100 + 0.5f),
                        (int)current, (int)((current - (int)current) * 100 + 0.5f),
                        mode_str, pdo_count, selected_pos, power_mw / 1000);
            
            // 如果缓冲区还有空间，添加统计信息
            if (n < maxlen - 50) {
                int additional_len = snprintf(buffer + n, maxlen - n, "%s  CC:%d PKT:%u CRC:%u\n",
                                            time_str, cc_pin, packet_count, crc_count);
                n += additional_len;
            }
            
            // 如果缓冲区还有更多空间，添加PDO详细信息
            if (n < maxlen - 80 && pdo_count > 0) {
                // 这里可以添加解析的PDO信息，但由于我们没有存储原始PDO数据，
                // 所以只显示基本信息
                int pdo_len = snprintf(buffer + n, maxlen - n, "%s  Power range: %u-%u mV, %u mA limit\n",
                                     time_str, pd_monitor.last_voltage, pd_monitor.last_voltage, pd_monitor.last_current);
                n += pdo_len;
            }
        } else {
            // 无连接时的输出
            n = snprintf(buffer, maxlen, "%sNo PD device connected\n", time_str);
        }
    }
    
    return n;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// v1.2修复：监听函数实现 - 返回实际数值
///////////////////////////////////////////////////////////////////////////////////////////////////

float PD_UFP_c::get_bridge_voltage(void)
{
    // v1.2修复：Bridge模式：返回监听数据中的值
    if (bridge_mode_enabled) {
        // 调试信息：显示内部状态
        static uint32_t last_debug = 0;
        if (millis() - last_debug > 3000) { // 每3秒输出一次调试
            Serial.printf("[DEBUG] get_bridge_voltage: last_voltage=%umV, power_status=%d, bridge_mode=%s\n",
                          pd_monitor.last_voltage, pd_monitor.power_status, bridge_mode_enabled ? "true" : "false");
            last_debug = millis();
        }
        
        if (pd_monitor.last_voltage > 0) {
            float voltage_v = pd_monitor.last_voltage / 1000.0f; // 转换为伏特
            return voltage_v;
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
    // v1.2修复：Bridge模式：返回监听数据中的值
    if (bridge_mode_enabled) {
        if (pd_monitor.last_current > 0) {
            float current_a = pd_monitor.last_current / 1000.0f; // 转换为安培
            return current_a;
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
        // 固定电压模式：10mA单位
        return ready_current * 0.01f; // 转换为安培
    }
    return 0.0f; // 无电源时返回0A
}

String PD_UFP_c::get_bridge_power_mode(void)
{
    // v1.2修复：Bridge模式：根据监听状态返回模式字符串
    if (bridge_mode_enabled) {
        switch (pd_monitor.power_status) {
            case STATUS_POWER_TYP:
                return "FIX";  // 固定电压模式
            case STATUS_POWER_PPS:
                return "PPS";  // 可编程电源模式
            case STATUS_POWER_NA:
            default:
                return "MON"; // 监听模式
        }
    }
}

uint32_t PD_UFP_c::get_bridge_packet_count(void)
{
    // 返回PD数据包计数
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
    // 重置监听数据
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
    if (bridge_mode_enabled && pd_monitor.cc_status && pd_monitor.src_cap_count > 0) {
        Serial.println("[FORCE_REFRESH] 强制刷新Bridge状态");
        Serial.printf("[FORCE_REFRESH] 当前状态: V=%umV, I=%umA, status=%d, position=%u\n",
                      pd_monitor.last_voltage, pd_monitor.last_current, pd_monitor.power_status, pd_monitor.selected_position);
        
        // 强制调用status_power_ready确保状态同步
        if (pd_monitor.power_status == STATUS_POWER_PPS && pd_monitor.last_voltage > 0 && pd_monitor.last_current > 0) {
            // PPS模式
            uint16_t voltage_raw = pd_monitor.last_voltage / 100; // 转换为100mV单位
            uint16_t current_raw = pd_monitor.last_current / 50;  // 转换为50mA单位
            uint16_t ready_voltage_raw = voltage_raw * 5;  // 100mV -> 20mV单位
            uint16_t ready_current_raw = current_raw;     // 50mA单位
            status_power_ready(STATUS_POWER_PPS, ready_voltage_raw, ready_current_raw);
        } else if (pd_monitor.power_status == STATUS_POWER_TYP && pd_monitor.last_voltage > 0 && pd_monitor.last_current > 0) {
            // Fixed/Variable/Battery模式
            uint16_t voltage_raw = pd_monitor.last_voltage / 50;  // 转换为50mV单位
            uint16_t current_raw = pd_monitor.last_current / 10;  // 转换为10mA单位
            status_power_ready(STATUS_POWER_TYP, voltage_raw, current_raw);
        }
        
        Serial.printf("[FORCE_REFRESH] 刷新后: V=%.3fV, I=%.3fA\n", 
                      get_bridge_voltage(), get_bridge_current());
    } else {
        Serial.println("[FORCE_REFRESH] Bridge模式未启用或无连接");
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// v1.2修复：监听模式下的电源信息获取函数实现
///////////////////////////////////////////////////////////////////////////////////////////////////

String PD_UFP_c::get_bridge_power_info_string(void)
{
    if (!bridge_mode_enabled) {
        return "Bridge模式未启用";
    }
    
    if (!pd_monitor.cc_status) {
        return "未检测到PD连接";
    }
    
    if (pd_monitor.src_cap_count == 0) {
        return "连接中，等待PD信息...";
    }
    
    String info = "PD电源信息:\n";
    info += "  电压: " + String(pd_monitor.last_voltage / 1000.0f, 2) + "V\n";
    info += "  电流: " + String(pd_monitor.last_current / 1000.0f, 2) + "A\n";
    info += "  功率: " + String((pd_monitor.last_voltage * pd_monitor.last_current) / 1000000.0f, 2) + "W\n";
    info += "  模式: " + get_bridge_power_mode() + "\n";
    info += "  PDO数量: " + String(pd_monitor.src_cap_count) + "\n";
    info += "  包计数: " + String(pd_monitor.packet_count) + "\n";
    
    return info;
}

uint32_t PD_UFP_c::get_bridge_max_power(void)
{
    if (!bridge_mode_enabled || !pd_monitor.cc_status) {
        return 0;
    }
    
    return (pd_monitor.last_voltage * pd_monitor.last_current) / 1000; // 转换为mW
}

uint16_t PD_UFP_c::get_bridge_voltage_range_min(void)
{
    if (!bridge_mode_enabled || !pd_monitor.cc_status) {
        return 0;
    }
    
    // 这里可以扩展为解析多个PDO获取电压范围
    // 目前返回当前电压值
    return pd_monitor.last_voltage;
}

uint16_t PD_UFP_c::get_bridge_voltage_range_max(void)
{
    if (!bridge_mode_enabled || !pd_monitor.cc_status) {
        return 0;
    }
    
    // 这里可以扩展为解析多个PDO获取电压范围
    // 目前返回当前电压值
    return pd_monitor.last_voltage;
}

uint16_t PD_UFP_c::get_bridge_current_limit(void)
{
    if (!bridge_mode_enabled || !pd_monitor.cc_status) {
        return 0;
    }
    
    return pd_monitor.last_current;
}

bool PD_UFP_c::is_bridge_pps_capable(void)
{
    if (!bridge_mode_enabled || !pd_monitor.cc_status || pd_monitor.src_cap_count == 0) {
        return false;
    }
    
    return (pd_monitor.power_status == STATUS_POWER_PPS);
}