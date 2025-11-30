
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
    // 更新监听数据
    pd_monitor.last_voltage = voltage;
    pd_monitor.last_current = current;
    pd_monitor.power_status = status;
    pd_monitor.last_timestamp = clock_ms();
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
    // 更新基础监听信息
    pd_monitor.last_voltage = get_voltage();      // 使用getter方法访问
    pd_monitor.last_current = get_current();      // 使用getter方法访问
    pd_monitor.last_timestamp = clock_ms();
    pd_monitor.power_status = get_ps_status();    // 使用getter方法访问
    
    // 获取当前CC线状态
    pd_monitor.cc_pin = get_cc_pin();
    pd_monitor.cc_status = (pd_monitor.cc_pin != 0);
    
    // 获取PD状态信息
    pd_monitor.src_cap_count = get_src_cap_count();
    pd_monitor.selected_position = get_selected_position();
    
    // Bridge模式下额外更新
    if (bridge_mode_enabled) {
        // 在Bridge模式下，包计数由事件处理单独更新，避免重复计数
        // 这里可以添加其他Bridge模式特定的更新逻辑
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
    
    // Initialize FUSB302 for bridge mode
    pinMode(int_pin, INPUT_PULLUP); // Set FUSB302 int pin input and pull up
    FUSB302.i2c_address = 0x22;
    FUSB302.i2c_read = FUSB302_i2c_read;
    FUSB302.i2c_write = FUSB302_i2c_write;
    FUSB302.delay_ms = FUSB302_delay_ms;
    
    if (FUSB302_init(&FUSB302) == FUSB302_SUCCESS && FUSB302_get_ID(&FUSB302, 0, 0) == FUSB302_SUCCESS) {
        status_initialized = 1;
    }
    
    // Initialize PD protocol engine for bridge mode
    PD_protocol_init(&protocol);
    
    // Clear bridge log buffer
    memset(bridge_log_buffer, 0, sizeof(bridge_log_buffer));
    bridge_log_index = 0;
    
    // 初始化监听数据
    memset(&pd_monitor, 0, sizeof(pd_monitor_t));
    pd_monitor.last_timestamp = clock_ms();
}

void PD_UFP_c::run_Bridge(void)
{
    if (!bridge_mode_enabled) return;
    
    if (timer() || digitalRead(int_pin) == 0) {
        FUSB302_event_t FUSB302_events = 0;
        for (uint8_t i = 0; i < 3 && FUSB302_alert(&FUSB302, &FUSB302_events) != FUSB302_SUCCESS; i++) {}
        if (FUSB302_events) {
            handle_FUSB302_event(FUSB302_events);
        }
        
        // 更新监听信息
        update_monitor_info();
        
        // 记录PD包数量
        if (FUSB302_events & FUSB302_EVENT_RX_SOP) {
            pd_monitor.packet_count++;
        }
        if (FUSB302_events & FUSB302_EVENT_GOOD_CRC_SENT) {
            pd_monitor.good_crc_count++;
        }
    }
}

int PD_UFP_c::status_bridge_log_readline(char *buffer, int maxlen)
{
    if (!bridge_mode_enabled || !buffer || maxlen <= 0) {
        return 0;
    }
    
    // 格式化当前状态信息到日志缓冲区
    snprintf(bridge_log_buffer, sizeof(bridge_log_buffer),
             "V:%.2fV I:%.2fA M:%s P:%d CC:%d T:%lu\r\n",
             get_bridge_voltage(), get_bridge_current(),
             get_bridge_power_mode().c_str(), pd_monitor.packet_count,
             pd_monitor.cc_pin, pd_monitor.last_timestamp);
    
    // 将日志信息复制到用户缓冲区
    int len = strlen(bridge_log_buffer);
    if (len > maxlen - 1) {
        len = maxlen - 1;
    }
    
    strncpy(buffer, bridge_log_buffer, len);
    buffer[len] = '\0';
    
    return len;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// 监听函数实现 - 返回实际数值
///////////////////////////////////////////////////////////////////////////////////////////////////

uint16_t PD_UFP_c::get_bridge_voltage(void)
{
    // 将内部电压值转换为实际电压值（V）
    // 内部电压值以50mV为单位（PPS以20mV为单位）
    if (status_power == STATUS_POWER_PPS) {
        // PPS模式：20mV单位
        return ready_voltage;
    } else if (status_power == STATUS_POWER_TYP) {
        // 固定电压模式：50mV单位
        return ready_voltage;
    }
    return 0.0f; // 无电源时返回0V
}

uint16_t PD_UFP_c::get_bridge_current(void)
{
    // 将内部电流值转换为实际电流值（A）
    // 内部电流值以10mA为单位（PPS以50mA为单位）
    if (status_power == STATUS_POWER_PPS) {
        // PPS模式：50mA单位
        return ready_current;
    } else if (status_power == STATUS_POWER_TYP) {
        // 固定电压模式：10mA单位
        return ready_current;
    }
    return 0.0f; // 无电源时返回0A
}

String PD_UFP_c::get_bridge_power_mode(void)
{
    // 根据电源状态返回模式字符串
    switch (status_power) {
        case STATUS_POWER_TYP:
            return "FIX";  // 固定电压模式
        case STATUS_POWER_PPS:
            return "PPS";  // 可编程电源模式
        case STATUS_POWER_NA:
        default:
            return "N/A"; // 无电源
    }
}

uint8_t PD_UFP_c::get_bridge_packet_count(void)
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

///////////////////////////////////////////////////////////////////////////////////////////////////
// PD_UFP_Log_c Bridge功能方法实现
///////////////////////////////////////////////////////////////////////////////////////////////////

PD_UFP_Log_c::PD_UFP_Log_c(pd_log_level_t log_level) :
    status_log_level(log_level),
    status_log_counter(0)
{
    bridge_mode_enabled = false;
    bridge_log_index = 0;
    memset(bridge_status_buffer, 0, sizeof(bridge_status_buffer));
}

void PD_UFP_Log_c::init_Bridge(uint8_t int_pin)
{
    // 调用父类初始化
    PD_UFP_c::init_Bridge(int_pin);
    
    // 初始化Log相关Bridge功能
    memset(bridge_status_buffer, 0, sizeof(bridge_status_buffer));
    snprintf(bridge_status_buffer, sizeof(bridge_status_buffer),
             "Bridge Mode Init - FUSB302 INT Pin: %d\r\n", int_pin);
}

void PD_UFP_Log_c::run_Bridge(void)
{
    // 调用父类运行
    PD_UFP_c::run_Bridge();
    
    // Log级别的Bridge状态更新
    if (bridge_mode_enabled) {
        // 更新详细状态信息
        snprintf(bridge_status_buffer, sizeof(bridge_status_buffer),
                 "PD Status: %s | V: %.2fV | I: %.2fA | Packets: %d | CC: %d\r\n",
                 get_bridge_power_mode().c_str(),
                 get_bridge_voltage(), 
                 get_bridge_current(),
                 get_bridge_packet_count(),
                 pd_monitor.cc_pin);
    }
}

int PD_UFP_Log_c::status_bridge_log_readline(char *buffer, int maxlen)
{
    if (!buffer || maxlen <= 0) return 0;
    
    int len = 0;
    
    // 首先返回父类的基本状态信息
    len = PD_UFP_c::status_bridge_log_readline(buffer, maxlen);
    
    // 然后添加Log级别的详细信息
    if (len < maxlen - 1 && bridge_mode_enabled) {
        int remaining = maxlen - len - 1; // 留一个字符给终止符
        if (remaining > 0) {
            buffer[len] = '\n'; // 添加换行符分隔
            len++;
            int log_len = snprintf(buffer + len, remaining, "Status: %s", bridge_status_buffer);
            if (log_len > 0 && log_len < remaining) {
                len += log_len;
            }
        }
    }
    
    return len;
}
