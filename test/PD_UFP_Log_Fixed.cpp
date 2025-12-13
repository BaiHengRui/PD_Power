/**
 * PD_UFP_Log_Fixed.cpp - FUSB302日志系统修复版本
 *
 * 修复内容：
 * 1. 优化日志格式，替换易混淆的|和I字符
 * 2. 简化日志输出，提高可读性
 * 3. 优化Bridge模式日志处理
 * 4. 减少日志对系统性能的影响
 */

#include <stdint.h>
#include <string.h>

#include "PD_UFP_Fixed.h"

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
// Optional: PD_UFP_Log_c, extended from PD_UFP_c to provide logging function.
//           Asynchronous, minimal impact on PD timing.
///////////////////////////////////////////////////////////////////////////////////////////////////
#define STATUS_LOG_MASK         (sizeof(status_log) / sizeof(status_log[0]) - 1)
#define STATUS_LOG_OBJ_MASK     (sizeof(status_log_obj) / sizeof(status_log_obj[0]) - 1)

PD_UFP_Log_c::PD_UFP_Log_c(pd_log_level_t log_level, pd_bridge_log_level_t bridge_log_level):
    PD_UFP_c(),  // 调用基类构造函数
    status_log_write(0),
    status_log_read(0),
    status_log_counter(0),
    status_log_obj_read(0),
    status_log_obj_write(0),
    status_log_level(log_level),
    bridge_log_level(bridge_log_level)
{
    // 初始化Bridge相关变量
    bridge_mode_enabled = false;
    bridge_mode_confirmed = false;
    monitor_active = false;
    bridge_log_index = 0;
    // bridge_status_buffer在基类中声明，这里不需要重复初始化
}

uint8_t PD_UFP_Log_c::status_log_obj_add(uint16_t header, uint32_t * obj)
{
    if (obj) {
        uint8_t i, w = status_log_obj_write, r = status_log_obj_read;
        PD_msg_info_t info;
        PD_protocol_get_msg_info(header, &info);
        for (i = 0; i < info.num_of_obj && (uint8_t)(w - r) < STATUS_LOG_OBJ_MASK; i++) {
            status_log_obj[w++ & STATUS_LOG_OBJ_MASK] = obj[i];
        }
        status_log_obj_write = w;
        return i;
    }
    return 0;
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
    // 更新监听信息
    update_monitor_info();
}

// Optimize RAM usage on AVR MCU by allocate format string in program memory
#if defined(__AVR__)
#include <avr/pgmspace.h>
#define SNPRINTF snprintf_P
#define MY_PSTR(str) PSTR(str)
#else
#define SNPRINTF snprintf
#define MY_PSTR(str) str
#endif

#define LOG(format, ...) do { n = SNPRINTF(buffer, maxlen, MY_PSTR(format), ## __VA_ARGS__); } while (0)

int PD_UFP_Log_c::status_log_readline_msg(char * buffer, int maxlen, status_log_t * log)
{
    char * t = status_log_time;
    int n = 0;
    if (status_log_counter == 0) {
        // output message header
        char type = log->status == STATUS_LOG_MSG_TX ? 'T' : 'R';
        PD_msg_info_t info;
        PD_protocol_get_msg_info(log->msg_header, &info);
        if (status_log_level >= PD_LOG_LEVEL_VERBOSE) {
            const char * ext = info.extended ? "ext, " : "";
            LOG("%s%cX %s id=%d %sraw=0x%04X\n", t, type, info.name, info.id, ext, log->msg_header);
            if (info.num_of_obj) {
                status_log_counter++;
            }
        } else {
            LOG("%s%cX %s\n", t, type, info.name);
        }
    } else {
        // output object data
        int i = status_log_counter - 1;
        uint32_t obj = status_log_obj[status_log_obj_read++ & STATUS_LOG_OBJ_MASK];
        LOG("%s obj%d=0x%08lX\n", t, i, obj);
        if (++status_log_counter > log->obj_count) {
            status_log_counter = 0;
        }
    }
    return n;
}

int PD_UFP_Log_c::status_log_readline_src_cap(char * buffer, int maxlen)
{
    PD_power_info_t p;
    int n = 0;
    uint8_t i = status_log_counter;
    if (PD_protocol_get_power_info(&protocol, i, &p)) {
        const char * str_pps[] = {"", " BAT", " VAR", " PPS"};  /* PD_power_data_obj_type_t */
        char * t = status_log_time;
        uint8_t selected = PD_protocol_get_selected_power(&protocol);
        char min_v[8] = {0}, max_v[8] = {0}, power[8] = {0};
        if (p.min_v) SNPRINTF(min_v, sizeof(min_v)-1, PSTR("%d.%02dV-"), p.min_v / 20, (p.min_v * 5) % 100);
        if (p.max_v) SNPRINTF(max_v, sizeof(max_v)-1, PSTR("%d.%02dV"), p.max_v / 20, (p.max_v * 5) % 100);
        if (p.max_i) {
            SNPRINTF(power, sizeof(power)-1, PSTR("%d.%02dA"), p.max_i / 100, p.max_i % 100);
        }
        if (i == selected) {
            LOG("%s SRC_CAP %s%s%s%d <SELECTED>\n", t, min_v, max_v, power, i);
        } else {
            LOG("%s SRC_CAP %s%s%s%d\n", t, min_v, max_v, power, i);
        }
        status_log_counter++;
    } else {
        status_log_counter = 0;
    }
    return n;
}

int PD_UFP_Log_c::status_log_readline(char * buffer, int maxlen)
{
    status_log_t * log = &status_log[status_log_read & STATUS_LOG_MASK];
    switch (log->status) {
    case STATUS_LOG_MSG_TX:
    case STATUS_LOG_MSG_RX:
        if (status_log_level >= PD_LOG_LEVEL_INFO) {
            return status_log_readline_msg(buffer, maxlen, log);
        }
        break;
    case STATUS_LOG_SRC_CAP:
        if (status_log_level >= PD_LOG_LEVEL_INFO) {
            return status_log_readline_src_cap(buffer, maxlen);
        }
        break;
    case STATUS_LOG_DEV:
        if (status_log_level >= PD_LOG_LEVEL_INFO) {
            char * t = status_log_time;
            int n = 0;
            LOG("%s DEV Initialized\n", t);
            return n;
        }
        break;
    case STATUS_LOG_CC:
        if (status_log_level >= PD_LOG_LEVEL_INFO) {
            char * t = status_log_time;
            int n = 0;
            LOG("%s CC Status Updated\n", t);
            return n;
        }
        break;
    case STATUS_LOG_POWER_READY:
        if (status_log_level >= PD_LOG_LEVEL_INFO) {
            char * t = status_log_time;
            int n = 0;
            LOG("%s POWER READY\n", t);
            return n;
        }
        break;
    case STATUS_LOG_POWER_PPS_STARTUP:
        if (status_log_level >= PD_LOG_LEVEL_INFO) {
            char * t = status_log_time;
            int n = 0;
            LOG("%s POWER PPS STARTUP\n", t);
            return n;
        }
        break;
    case STATUS_LOG_POWER_REJECT:
        if (status_log_level >= PD_LOG_LEVEL_INFO) {
            char * t = status_log_time;
            int n = 0;
            LOG("%s POWER REJECT\n", t);
            return n;
        }
        break;
    case STATUS_LOG_LOAD_SW_ON:
        if (status_log_level >= PD_LOG_LEVEL_INFO) {
            char * t = status_log_time;
            int n = 0;
            LOG("%s LOAD SWITCH ON\n", t);
            return n;
        }
        break;
    case STATUS_LOG_LOAD_SW_OFF:
        if (status_log_level >= PD_LOG_LEVEL_INFO) {
            char * t = status_log_time;
            int n = 0;
            LOG("%s LOAD SWITCH OFF\n", t);
            return n;
        }
        break;
    default:
        break;
    }
    status_log_read++;
    return 0;
}

// 修复：Bridge模式初始化
void PD_UFP_Log_c::init_Bridge(uint8_t int_pin)
{
    Serial.println("[LOG] Bridge模式日志初始化");
    
    // 调用基类初始化
    PD_UFP_c::init_Bridge(int_pin);
    
    // 设置日志级别
    bridge_log_level = bridge_log_level; // 使用构造函数传入的级别
    
    Serial.printf("[LOG] Bridge日志级别: %s\n", 
                  bridge_log_level == PD_BRIDGE_LOG_LEVEL_DETAILED ? "详细" : "基础");
}

// 修复：Bridge模式运行
void PD_UFP_Log_c::run_Bridge(void)
{
    // 调用基类运行
    PD_UFP_c::run_Bridge();
    
    // 添加额外的日志处理逻辑
    if (bridge_log_level == PD_BRIDGE_LOG_LEVEL_DETAILED) {
        // 详细模式下可以添加额外的日志处理
        static uint32_t last_log_time = 0;
        if (millis() - last_log_time > 5000) { // 每5秒输出一次状态
            last_log_time = millis();
            
            if (monitor_active && pd_monitor.cc_status) {
                Serial.printf("[LOG] 状态: %.3fV %.3fA [%s] 包数:%u\n",
                             get_bridge_voltage(), get_bridge_current(),
                             get_bridge_power_mode().c_str(),
                             get_bridge_packet_count());
            }
        }
    }
}

// 修复：Bridge日志读取函数
int PD_UFP_Log_c::status_bridge_log_readline(char *buffer, int maxlen)
{
    // 调用基类方法
    int result = PD_UFP_c::status_bridge_log_readline(buffer, maxlen);
    
    // 添加额外的信息
    if (bridge_log_level == PD_BRIDGE_LOG_LEVEL_DETAILED && result > 0) {
        // 在详细模式下，添加更多状态信息
        char temp_buffer[128];
        int temp_len = 0;
        
        if (monitor_active) {
            temp_len = snprintf(temp_buffer, sizeof(temp_buffer), " CC%d PS:%d",
                               get_bridge_cc_pin(),
                               get_bridge_power_status());
            
            if (result + temp_len < maxlen) {
                strncat(buffer, temp_buffer, maxlen - result - 1);
                result += temp_len;
            }
        }
    }
    
    return result;
}

// 修复：状态打印函数
void PD_UFP_Log_c::print_status(HardwareSerial & serial)
{
    serial.println("=== PD Status Report ===");
    
    // 基本状态
    serial.printf("Bridge Mode: %s\n", bridge_mode_enabled ? "ON" : "OFF");
    serial.printf("Monitor Active: %s\n", monitor_active ? "YES" : "NO");
    serial.printf("CC Status: %s\n", get_bridge_cc_status() ? "Connected" : "Disconnected");
    
    if (bridge_mode_enabled && monitor_active) {
        // 电源信息
        serial.printf("Power: %.3fV %.3fA [%s]\n", 
                     get_bridge_voltage(), get_bridge_current(),
                     get_bridge_power_mode().c_str());
        
        // 统计数据
        serial.printf("Packets: %u\n", get_bridge_packet_count());
        serial.printf("Good CRC: %u\n", get_bridge_good_crc_count());
        serial.printf("Source Caps: %u\n", get_bridge_src_cap_count());
        serial.printf("Selected Position: %u\n", get_bridge_selected_position());
        
        // PPS能力
        if (is_bridge_pps_capable()) {
            serial.printf("PPS Range: %umV-%umV, Max Current: %umA\n",
                         get_bridge_voltage_range_min(),
                         get_bridge_voltage_range_max(),
                         get_bridge_current_limit());
        }
        
        // CC线信息
        uint8_t cc_pin = get_bridge_cc_pin();
        if (cc_pin > 0) {
            serial.printf("CC Pin: CC%d\n", cc_pin);
        }
    } else {
        serial.println("Monitor: Waiting for connection...");
    }
    
    serial.println("========================");
}

// 重写update_monitor_info函数以添加日志
void PD_UFP_Log_c::update_monitor_info(void)
{
    // Bridge模式下不需要调用基类的update_monitor_info
    // 因为我们在handle_FUSB302_bridge_event中直接更新状态
    
    // 可以在这里添加额外的监控逻辑
    if (bridge_mode_enabled && monitor_active) {
        // 检查状态变化
        static uint16_t last_voltage = 0;
        static uint16_t last_current = 0;
        static status_power_t last_status = STATUS_POWER_NA;
        
        if (pd_monitor.last_voltage != last_voltage ||
            pd_monitor.last_current != last_current ||
            pd_monitor.power_status != last_status) {
            
            // 状态发生了变化
            Serial.printf("[LOG] 电源状态变化: %.3fV -> %.3fV, %.3fA -> %.3fA, %s -> %s\n",
                         last_voltage/1000.0f, pd_monitor.last_voltage/1000.0f,
                         last_current/1000.0f, pd_monitor.last_current/1000.0f,
                         last_status == STATUS_POWER_PPS ? "PPS" : 
                         last_status == STATUS_POWER_TYP ? "FIX" : "NA",
                         pd_monitor.power_status == STATUS_POWER_PPS ? "PPS" : 
                         pd_monitor.power_status == STATUS_POWER_TYP ? "FIX" : "NA");
            
            last_voltage = pd_monitor.last_voltage;
            last_current = pd_monitor.last_current;
            last_status = pd_monitor.power_status;
        }
    }
}