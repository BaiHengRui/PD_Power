
/**
 * PD_UFP.h
 *
 *      Author: Ryan Ma
 *      Edited: Kai Liebich
 *
 * Minimalist USB PD Ardunio Library for PD Micro board
 * Only support UFP(device) sink only functionality
 * Requires FUSB302_UFP.h, PD_UFP_Protocol.h and Standard Arduino Library
 *
 * Support PD3.0 PPS
 * 
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
    // 初始化监听数据
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
        PD_protocol_reset(&protocol);
        
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
        PD_protocol_reset(&protocol);
        if (cc1 && cc2 == 0) {
            cc = cc1;
        } else if (cc2 && cc1 == 0) {
            cc = cc2;
        }
        /* TODO: handle no cc detected error */
        if (cc > 1) {
            wait_src_cap = 1;
        } else {
            set_default_power();
        }
        
        // Bridge模式下的CC状态更新
        if (bridge_mode_enabled) {
            pd_monitor.cc_status = true;
            pd_monitor.cc_pin = cc;
        }
        
        status_log_event(STATUS_LOG_CC);
    }
    if (events & FUSB302_EVENT_RX_SOP) {
        PD_protocol_event_t protocol_event = 0;
        uint16_t header;
        uint32_t obj[7];
        FUSB302_get_message(&FUSB302, &header, obj);
        PD_protocol_handle_msg(&protocol, header, obj, &protocol_event);
        
        // Bridge模式下的数据包计数
        if (bridge_mode_enabled) {
            pd_monitor.packet_count++;
        }
        
        status_log_event(STATUS_LOG_MSG_RX, obj);
        if (protocol_event) {
            handle_protocol_event(protocol_event);
        }
    }
    if (events & FUSB302_EVENT_GOOD_CRC_SENT) {
        uint16_t header;
        uint32_t obj[7];
        delay_ms(2);  /* Delay respond in case there are retry messages */
        if (PD_protocol_respond(&protocol, &header, obj)) {
            status_log_event(STATUS_LOG_MSG_TX, obj);
            FUSB302_tx_sop(&FUSB302, header, obj);
        }
        
        // Bridge模式下的Good CRC计数
        if (bridge_mode_enabled) {
            pd_monitor.good_crc_count++;
        }
    }
}

bool PD_UFP_c::timer(void)
{
    uint16_t t = clock_ms();
    if (wait_src_cap && t - time_wait_src_cap > t_TypeCSinkWaitCap) {
        time_wait_src_cap = t;
        if (get_src_cap_retry_count < 3) {
            uint16_t header;
            get_src_cap_retry_count += 1;
            /* Try to request soruce capabilities message (will not cause power cycle VBUS) */
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
    ready_voltage = voltage;
    ready_current = current;
    status_power = status;
    
    // Bridge模式下的单位转换修复
    if (bridge_mode_enabled) {
        // Bridge模式下，voltage和current是原始单位，需要转换为mV和mA
        if (status == STATUS_POWER_PPS) {
            // PPS模式：ready_voltage是20mV单位，ready_current是50mA单位
            // voltage是20mV单位的原始值，current是50mA单位的原始值
            pd_monitor.last_voltage = voltage * 20;  // 20mV单位转换为mV
            pd_monitor.last_current = current * 50;  // 50mA单位转换为mA
        } else {
            // Fixed/Variable/Battery模式：ready_voltage是50mV单位，ready_current是10mA单位
            // voltage是50mV单位的原始值，current是10mA单位的原始值
            pd_monitor.last_voltage = voltage * 50;  // 50mV单位转换为mV
            pd_monitor.last_current = current * 10;  // 10mA单位转换为mA
        }
    } else {
        // 非Bridge模式：直接更新监听数据
        pd_monitor.last_voltage = voltage;
        pd_monitor.last_current = current;
    }
    
    pd_monitor.power_status = status;
    pd_monitor.last_timestamp = clock_ms();
    
    // 调试输出
    if (bridge_mode_enabled) {
        Serial.printf("[STATUS] Bridge模式状态更新: status=%d, V_raw=%u, I_raw=%u, V_mV=%umV, I_mA=%umA\n",
                      status, voltage, current, pd_monitor.last_voltage, pd_monitor.last_current);
    }
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
        // Bridge模式：只更新时间戳，不访问协议状态
        // 所有状态更新都在run_Bridge()中的事件处理里完成
        
        // Bridge模式：确保内部状态与监听数据同步
        // Bridge模式下的状态已在status_power_ready()中正确更新
        
        // 如果CC已连接但还没有PDO信息，保持当前状态
        // 不主动修改power_status，避免与PDO解析结果冲突
        
        // Serial.printf("[UPDATE] Bridge模式: 保持当前状态\n");
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
    
    // 调试输出：显示update_monitor_info的结果（仅在状态变化时）
    static status_power_t last_power_status = STATUS_POWER_NA;
    static uint16_t last_voltage = 0;
    static uint16_t last_current = 0;
    
    if (bridge_mode_enabled && 
        (last_power_status != pd_monitor.power_status || 
         last_voltage != pd_monitor.last_voltage || 
         last_current != pd_monitor.last_current)) {
        Serial.printf("[UPDATE] Bridge模式状态变化: V=%umV, I=%umA, power_status=%d, CC=%d\n",
                      pd_monitor.last_voltage, pd_monitor.last_current, pd_monitor.power_status, pd_monitor.cc_status);
        last_power_status = pd_monitor.power_status;
        last_voltage = pd_monitor.last_voltage;
        last_current = pd_monitor.last_current;
    }
}

void PD_UFP_c::reset_monitor_info(void)
{
    memset(&pd_monitor, 0, sizeof(pd_monitor_t));
    pd_monitor.last_timestamp = clock_ms();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Bridge功能方法实现
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
    
    // Bridge模式：不初始化PD协议引擎，避免任何协议处理
    // PD_protocol_init(&protocol);  // 注释掉协议初始化
    
    // Clear bridge log buffer
    memset(bridge_log_buffer, 0, sizeof(bridge_log_buffer));
    bridge_log_index = 0;
    
    // 初始化监听数据
    memset(&pd_monitor, 0, sizeof(pd_monitor_t));
    pd_monitor.last_timestamp = clock_ms();
    
    // 设置默认的监听状态
    pd_monitor.power_status = STATUS_POWER_NA;  // 未知电源状态
    pd_monitor.selected_position = 1;  // 默认选择第一个电源（位置1）
    
    // 关键修复：初始化内部状态
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
    
    // 移除timer()调用，避免无限循环重启握手
    // 只在有中断或定时轮询时才处理
    if (digitalRead(int_pin) == 0) {
        FUSB302_event_t FUSB302_events = 0;
        for (uint8_t i = 0; i < 3 && FUSB302_alert(&FUSB302, &FUSB302_events) != FUSB302_SUCCESS; i++) {}
        
        if (FUSB302_events) {
            // 纯监听模式：只记录事件，不进行PD握手
            
            if (FUSB302_events & FUSB302_EVENT_DETACHED) {
                // 设备断开连接
                Serial.println("[BRIDGE] 设备断开连接");
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
                
                // 关键修复：调用status_power_ready重置状态
                status_power_ready(STATUS_POWER_NA, 0, 0);
            }
            
            if (FUSB302_events & FUSB302_EVENT_ATTACHED) {
                // 设备连接，只更新监听状态，不触发PD握手
                Serial.println("[BRIDGE] 设备连接");
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
            }
            
            if (FUSB302_EVENT_RX_SOP & FUSB302_events) {
                // 接收到PD数据包，只统计不处理协议
                uint16_t header;
                uint32_t obj[7];
                FUSB302_get_message(&FUSB302, &header, obj);
                
                pd_monitor.packet_count++;
                
                // 解析PDO信息，获取电压电流信息
                uint16_t msg_header = header;
                uint8_t num_obj = (msg_header >> 12) & 0x07;
                uint8_t msg_type = (msg_header >> 0) & 0x1F; // 修正：使用5位而不是4位
                
                // 调试信息：打印接收到的消息信息
                Serial.printf("[DEBUG] RX: header=0x%04X, num_obj=%d, msg_type=%d\n", 
                              header, num_obj, msg_type);
                for (uint8_t i = 0; i < num_obj && i < 3; i++) {
                    Serial.printf("[DEBUG]   obj[%d]=0x%08lX\n", i, obj[i]);
                }
                
                // 如果是Source Capabilities消息，解析PDO信息
                // 修正：Source Capabilities在data_msg_list中索引为1，所以类型为0x01
                if (num_obj > 0 && msg_type == 0x01) {
                    Serial.println("[DEBUG] Source Capabilities received!");
                    // Source Capabilities消息，解析所有PDO
                    pd_monitor.src_cap_count = num_obj;
                    // 在监听模式下，我们假设选择第一个电源（位置1）
                    pd_monitor.selected_position = 1;
                    
                    // 解析第一个PDO作为默认电源信息
                    if (num_obj > 0) {
                        uint32_t first_pdo = obj[0];
                        uint8_t pdo_type = (first_pdo >> 30) & 0x03;
                        
                        Serial.printf("[DEBUG] PDO0=0x%08lX, type=%d (当前power_status=%d)\n", 
                                      first_pdo, pdo_type, pd_monitor.power_status);
                        
                        switch (pdo_type) {
                            case 0: // Fixed Supply
                                {
                                    uint16_t voltage_raw = ((first_pdo >> 10) & 0x3FF);
                                    uint16_t current_raw = ((first_pdo >> 0) & 0x3FF);
                                    pd_monitor.power_status = STATUS_POWER_TYP;
                                    Serial.printf("[BRIDGE] Fixed Supply: raw_v=%u(50mV), raw_i=%u(10mA)\n", 
                                                  voltage_raw, current_raw);
                                    
                                    // 关键修复：只调用status_power_ready，让它处理单位转换
                                    status_power_ready(STATUS_POWER_TYP, voltage_raw, current_raw);
                                    Serial.printf("[BRIDGE] Fixed Supply: V=%umV, I=%umA\n", 
                                                  pd_monitor.last_voltage, pd_monitor.last_current);
                                }
                                break;
                                
                            case 1: // Battery
                                {
                                    uint16_t max_voltage_raw = ((first_pdo >> 20) & 0x3FF);
                                    uint16_t power_raw = ((first_pdo >> 0) & 0x3FF);
                                    uint16_t max_voltage_mv = max_voltage_raw * 50; // 最大电压
                                    uint16_t max_current_ma = (power_raw * 250000) / max_voltage_mv; // 功率转换为电流 (mA)
                                    pd_monitor.power_status = STATUS_POWER_TYP;
                                    Serial.printf("[BRIDGE] Battery: raw_v=%u(50mV), Power=%u*250mW, calc_i=%umA\n", 
                                                  max_voltage_raw, power_raw, max_current_ma);
                                    
                                    // 关键修复：调用status_power_ready更新状态
                                    // Battery模式转换为Fixed模式：使用最大电压和计算得到的电流
                                    uint16_t current_raw = max_current_ma / 10; // 转换为10mA单位
                                    status_power_ready(STATUS_POWER_TYP, max_voltage_raw, current_raw);
                                    Serial.printf("[BRIDGE] Battery: V=%umV, I=%umA\n", 
                                                  pd_monitor.last_voltage, pd_monitor.last_current);
                                }
                                break;
                                
                            case 2: // Variable Supply
                                {
                                    uint16_t max_voltage_raw = ((first_pdo >> 20) & 0x3FF);
                                    uint16_t max_current_raw = ((first_pdo >> 0) & 0x3FF);
                                    pd_monitor.power_status = STATUS_POWER_TYP;
                                    Serial.printf("[BRIDGE] Variable: raw_v=%u(50mV), raw_i=%u(10mA)\n",
                                                  max_voltage_raw, max_current_raw);
                                    
                                    // 关键修复：调用status_power_ready更新状态
                                    // Variable模式直接使用原始值（与Fixed模式相同的单位）
                                    status_power_ready(STATUS_POWER_TYP, max_voltage_raw, max_current_raw);
                                    Serial.printf("[BRIDGE] Variable: V=%umV, I=%umA\n",
                                                  pd_monitor.last_voltage, pd_monitor.last_current);
                                }
                                break;
                                
                            case 3: // Augmented PDO (PPS)
                                {
                                    // 修正：根据USB PD 3.0规范，正确的位域提取
                                    uint16_t voltage_raw = ((first_pdo >> 17) & 0x7FF);  // 11位，位[17:7]
                                    uint16_t current_raw = ((first_pdo >> 7) & 0xFF);    // 8位，位[14:7]
                                    pd_monitor.power_status = STATUS_POWER_PPS;
                                    Serial.printf("[BRIDGE] PPS: raw_v=%u(100mV), raw_i=%u(50mA)\n", 
                                                  voltage_raw, current_raw);
                                    
                                    // 关键修复：调用status_power_ready更新状态
                                    // PPS模式：ready_voltage使用20mV单位，ready_current使用50mA单位
                                    uint16_t ready_voltage_raw = voltage_raw * 5;  // 100mV -> 20mV单位 (乘以5)
                                    uint16_t ready_current_raw = current_raw;     // 50mA单位保持不变
                                    status_power_ready(STATUS_POWER_PPS, ready_voltage_raw, ready_current_raw);
                                    Serial.printf("[BRIDGE] PPS: V=%umV, I=%umA\n", 
                                                  pd_monitor.last_voltage, pd_monitor.last_current);
                                }
                                break;
                                
                            default:
                                Serial.printf("[DEBUG] Unknown PDO type: %d\n", pdo_type);
                                break;
                        }
                    }
                    
                    // 如果有多个PDO，选择功率最大的作为"默认"电源
                    uint16_t max_power_voltage = 0;
                    uint16_t max_power_current = 0;
                    uint32_t max_power = 0;
                    status_power_t max_power_type = STATUS_POWER_NA;  // 记录最大功率PDO的类型
                    
                    for (uint8_t i = 0; i < num_obj && i < 7; i++) {
                        uint32_t pdo = obj[i];
                        uint8_t type = (pdo >> 30) & 0x03;
                        uint16_t voltage_mv = 0;
                        uint16_t current_ma = 0;
                        uint32_t power_mw = 0;
                        
                        switch (type) {
                            case 0: // Fixed Supply
                                {
                                    uint16_t v_raw = ((pdo >> 10) & 0x3FF);
                                    uint16_t i_raw = ((pdo >> 0) & 0x3FF);
                                    voltage_mv = v_raw * 50;
                                    current_ma = i_raw * 10;
                                    power_mw = voltage_mv * current_ma / 1000;
                                }
                                break;
                            case 2: // Variable Supply
                                {
                                    uint16_t v_raw = ((pdo >> 20) & 0x3FF);
                                    uint16_t i_raw = ((pdo >> 0) & 0x3FF);
                                    voltage_mv = v_raw * 50;
                                    current_ma = i_raw * 10;
                                    power_mw = voltage_mv * current_ma / 1000;
                                }
                                break;
                            case 3: // PPS
                                {
                                    // 修正：根据USB PD 3.0规范，正确的位域提取
                                    uint16_t v_raw = ((pdo >> 17) & 0x7FF);  // 11位，位[17:7]
                                    uint16_t i_raw = ((pdo >> 7) & 0xFF);    // 8位，位[14:7]
                                    voltage_mv = v_raw * 100;
                                    current_ma = i_raw * 50;
                                    power_mw = voltage_mv * current_ma / 1000;
                                }
                                break;
                        }
                        
                        Serial.printf("[DEBUG] PDO[%d]: Type=%d, V=%umV, I=%umA, P=%umW\n",
                                      i, type, voltage_mv, current_ma, power_mw);
                        
                        if (power_mw > max_power) {
                            max_power = power_mw;
                            max_power_voltage = voltage_mv;
                            max_power_current = current_ma;
                            max_power_type = (type == 3) ? STATUS_POWER_PPS : STATUS_POWER_TYP;  // 根据PDO类型设置
                        }
                    }
                    
                    // Bridge模式：选择第一个PDO（通常为5V默认档）而不是最大功率
                    // 这样更符合实际使用场景，用户期望看到默认的5V电压
                    uint32_t first_pdo = obj[0];
                    uint8_t first_type = (first_pdo >> 30) & 0x03;
                    uint16_t first_voltage_mv = 0;
                    uint16_t first_current_ma = 0;
                    status_power_t first_power_type = STATUS_POWER_NA;
                    
                    // 解析第一个PDO
                    switch (first_type) {
                        case 0: // Fixed Supply
                            {
                                uint16_t v_raw = ((first_pdo >> 10) & 0x3FF);
                                uint16_t i_raw = ((first_pdo >> 0) & 0x3FF);
                                first_voltage_mv = v_raw * 50;
                                first_current_ma = i_raw * 10;
                                first_power_type = STATUS_POWER_TYP;
                            }
                            break;
                        case 1: // Battery
                            {
                                uint16_t v_raw = ((first_pdo >> 20) & 0x3FF);
                                uint16_t p_raw = ((first_pdo >> 0) & 0x3FF);
                                first_voltage_mv = v_raw * 50;
                                first_current_ma = (p_raw * 250000) / first_voltage_mv;
                                first_power_type = STATUS_POWER_TYP;
                            }
                            break;
                        case 2: // Variable Supply
                            {
                                uint16_t v_raw = ((first_pdo >> 20) & 0x3FF);
                                uint16_t i_raw = ((first_pdo >> 0) & 0x3FF);
                                first_voltage_mv = v_raw * 50;
                                first_current_ma = i_raw * 10;
                                first_power_type = STATUS_POWER_TYP;
                            }
                            break;
                        case 3: // PPS
                            {
                                uint16_t v_raw = ((first_pdo >> 17) & 0x7FF);
                                uint16_t i_raw = ((first_pdo >> 7) & 0xFF);
                                first_voltage_mv = v_raw * 100;
                                first_current_ma = i_raw * 50;
                                first_power_type = STATUS_POWER_PPS;
                            }
                            break;
                    }
                    
                    Serial.printf("[BRIDGE] 选择第一个PDO (默认档): V=%umV, I=%umA, Type=%d\n", 
                                  first_voltage_mv, first_current_ma, first_power_type);
                    
                    // 调用status_power_ready更新为第一个PDO的值
                    uint16_t ready_voltage_raw = 0;
                    uint16_t ready_current_raw = 0;
                    
                    if (first_power_type == STATUS_POWER_PPS) {
                        // PPS模式：ready_voltage使用20mV单位，ready_current使用50mA单位
                        ready_voltage_raw = first_voltage_mv / 20;
                        ready_current_raw = first_current_ma / 50;
                    } else {
                        // Fixed/Variable/Battery模式：ready_voltage使用50mV单位，ready_current使用10mA单位
                        ready_voltage_raw = first_voltage_mv / 50;
                        ready_current_raw = first_current_ma / 10;
                    }
                    
                    // 更新状态为第一个PDO
                    status_power_ready(first_power_type, ready_voltage_raw, ready_current_raw);
                    Serial.printf("[BRIDGE] 第一个PDO已设置: V=%umV, I=%umA\n",
                                  pd_monitor.last_voltage, pd_monitor.last_current);
                    
                    // 如果需要查看最大功率信息，可以保留但不选择
                    if (max_power > 0) {
                        Serial.printf("[BRIDGE] 最大功率PDO参考: V=%umV, I=%umA, P=%umW (未选择)\n", 
                                      max_power_voltage, max_power_current, max_power);
                    }
                }
                
                // 如果接收到Request消息，只记录不更新Bridge模式状态
                // 修正：Request在data_msg_list中索引为2，所以类型为0x02
                else if (num_obj > 0 && msg_type == 0x02) {
                    Serial.println("[BRIDGE] Request message received (仅监听，不更新状态)!");
                    // Request消息，从设备请求的电源
                    uint32_t request_pdo = obj[0];
                    uint8_t request_type = (request_pdo >> 30) & 0x03;
                    uint16_t voltage = 0;
                    uint16_t current = 0;
                    
                    // 正确解析Request PDO的电源类型和值
                    switch (request_type) {
                        case 0: // Fixed Supply Request
                            voltage = ((request_pdo >> 10) & 0x3FF) * 50; // 50mV单位
                            current = ((request_pdo >> 0) & 0x3FF) * 10;  // 10mA单位
                            break;
                        case 3: // PPS Request
                            voltage = ((request_pdo >> 17) & 0x7FF) * 100; // 100mV单位
                            current = ((request_pdo >> 7) & 0xFF) * 50;    // 50mA单位
                            break;
                    }
                    
                    // Bridge模式：只记录Request信息，不更新监听状态
                    // 因为Bridge模式不参与实际的PD握手，只监听Source Capabilities中声明的电源
                    Serial.printf("[BRIDGE] Request (仅记录): V=%umV, I=%umA, Type=%d\n", 
                                  voltage, current, request_type);
                    Serial.printf("[BRIDGE] Bridge模式保持Source Capabilities电压: V=%umV, I=%umA\n",
                                  pd_monitor.last_voltage, pd_monitor.last_current);
                }
                
                // 重要：不调用PD_protocol_handle_msg，避免触发协议处理
                // 重要：不调用handle_protocol_event，避免触发PD握手
            }
            
            if (FUSB302_events & FUSB302_EVENT_GOOD_CRC_SENT) {
                // Good CRC发送成功，只统计不响应
                pd_monitor.good_crc_count++;
                
                // 重要：不调用PD_protocol_respond，避免发送响应消息
                // 重要：不调用FUSB302_tx_sop，避免发送任何PD消息
            }
        }
        
        // 更新监听信息
        update_monitor_info();
    }
}

int PD_UFP_c::status_bridge_log_readline(char *buffer, int maxlen)
{
    // 增强的安全检查
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
// 监听函数实现 - 返回实际数值
///////////////////////////////////////////////////////////////////////////////////////////////////

float PD_UFP_c::get_bridge_voltage(void)
{
    // Bridge模式：优先使用监听数据中的值
    if (bridge_mode_enabled) {
        // 调试：每次调用都输出，便于跟踪问题
        Serial.printf("[GET_VOLTAGE] Bridge模式: last_voltage=%umV, power_status=%d\n", 
                      pd_monitor.last_voltage, pd_monitor.power_status);
        
        if (pd_monitor.last_voltage > 0) {
            float voltage_v = pd_monitor.last_voltage / 1000.0f; // 转换为伏特（监听数据已经是mV）
            Serial.printf("[GET_VOLTAGE] -> 返回 %.3fV\n", voltage_v);
            return voltage_v;
        }
        
        Serial.printf("[GET_VOLTAGE] -> 返回 0V (last_voltage=%u)\n", pd_monitor.last_voltage);
        return 0.0f; // 监听模式下无电压信息时返回0V
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
    // Bridge模式：优先使用监听数据中的值
    if (bridge_mode_enabled) {
        // 调试：每次调用都输出，便于跟踪问题
        Serial.printf("[GET_CURRENT] Bridge模式: last_current=%umA, power_status=%d\n", 
                      pd_monitor.last_current, pd_monitor.power_status);
        
        if (pd_monitor.last_current > 0) {
            float current_a = pd_monitor.last_current / 1000.0f; // 转换为安培（监听数据已经是mA）
            Serial.printf("[GET_CURRENT] -> 返回 %.3fA\n", current_a);
            return current_a;
        }
        
        Serial.printf("[GET_CURRENT] -> 返回 0A (last_current=%u)\n", pd_monitor.last_current);
        return 0.0f; // 监听模式下无电流信息时返回0A
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
    // Bridge模式：根据监听状态返回模式字符串
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

///////////////////////////////////////////////////////////////////////////////////////////////////
// 监听模式下的电源信息获取函数实现
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


