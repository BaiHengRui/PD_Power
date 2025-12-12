/**
 * PD_UFP.h
 *
 *      Author: Ryan Ma
 *      Edited: Kai Liebich
 *      v1.2: Bridge模式完整修复
 *
 * Minimalist USB PD Ardunio Library for PD Micro board
 * Only support UFP(device) sink only functionality
 * Requires FUSB302_UFP.h, PD_UFP_Protocol.h and Standard Arduino Library
 *
 * Support PD3.0 PPS
 * 
 * v1.2修复内容：
 * - 完全修复Bridge模式循环握手问题
 * - 修复Request PDO解析逻辑
 * - 修复PPS模式电压电流解析
 * - 优化消息处理和状态更新
 * - 提供详细的PD消息监控
 */

#ifndef PD_UFP_H
#define PD_UFP_H

#include <stdint.h>

#include <Arduino.h>
#include <Wire.h>
#include <HardwareSerial.h>

#include "FUSB302_UFP.h"
#include "PD_UFP_Protocol.h"

enum {
    STATUS_POWER_NA = 0,
    STATUS_POWER_TYP,
    STATUS_POWER_PPS
};
typedef uint8_t status_power_t;

// Bridge日志级别枚举 - 必须在类定义之前声明
enum pd_bridge_log_level_t {
    PD_BRIDGE_LOG_LEVEL_BASIC,      // 基础信息
    PD_BRIDGE_LOG_LEVEL_DETAILED    // 详细信息
};

// 监听功能数据结构
struct pd_monitor_t {
    uint32_t packet_count;          // 数据包计数
    uint32_t good_crc_count;        // 成功CRC计数
    uint32_t reject_count;          // 拒绝计数
    uint16_t last_voltage;          // 最后一次电压
    uint16_t last_current;          // 最后一次电流
    uint32_t last_timestamp;        // 最后时间戳
    bool cc_status;                 // CC线状态
    uint8_t cc_pin;                 // CC引脚状态
    uint8_t src_cap_count;          // 源能力数量
    uint8_t selected_position;      // 选择位置
    status_power_t power_status;    // 电源状态
    // v1.2修复：缓存Source Capabilities的PDO
    uint32_t source_pdos[7];        // 缓存的源PDO列表（最多7个）
    uint8_t source_pdo_count;       // 实际PDO数量
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// PD_UFP_c
///////////////////////////////////////////////////////////////////////////////////////////////////
class PD_UFP_c
{
    public:
        PD_UFP_c();
        // Init
        void init(uint8_t int_pin, enum PD_power_option_t power_option = PD_POWER_OPTION_MAX_5V);
        void init_PPS(uint8_t int_pin, uint16_t PPS_voltage, uint8_t PPS_current, enum PD_power_option_t power_option = PD_POWER_OPTION_MAX_5V);
        bool enable_vbus_sense(bool enable = 1);  //VBUS 检测
        // Task
        void run(void);
        // Status
        bool is_power_ready(void) { return status_power == STATUS_POWER_TYP; }
        bool is_PPS_ready(void)   { return status_power == STATUS_POWER_PPS; }
        bool is_ps_transition(void) { return send_request || wait_ps_rdy; }
        // Get
        uint16_t get_voltage(void) { return ready_voltage; }    // Voltage in 50mV units, 20mV(PPS)
        uint16_t get_current(void) { return ready_current; }    // Current in 10mA units, 50mA(PPS)
        status_power_t get_ps_status(void) { return status_power; }
        
        uint8_t get_src_cap_count(void) { return protocol.power_data_obj_count; }
        uint8_t get_selected_position(void) { int Position = 1+ (PD_protocol_get_selected_power(&protocol)); return Position;}
        uint8_t get_cc_pin();
        
        // v1.2修复：监听功能方法
        pd_monitor_t get_monitor_info(void) { return pd_monitor; }
        void reset_monitor_info(void);
        
        // 监听函数 - 返回实际数值
        float get_bridge_voltage(void);  // 返回float类型电压值（V）
        float get_bridge_current(void);  // 返回float类型电流值（A）
        String get_bridge_power_mode(void);  // 返回电源模式："FIX"或"PPS"
        uint32_t get_bridge_packet_count(void);   // 返回PD包数量
        
        // 新的Bridge监听函数
        uint8_t get_bridge_src_cap_count(void);      // 获取源能力计数
        uint8_t get_bridge_selected_position(void);  // 获取PD位置
        uint8_t get_bridge_cc_pin(void);             // 获取CC线状态
        uint32_t get_bridge_good_crc_count(void);    // 获取成功CRC计数
        uint32_t get_bridge_reject_count(void);      // 获取拒绝计数
        bool get_bridge_cc_status(void);             // 获取CC线连接状态
        void reset_bridge_monitor(void);             // 重置监听数据
        void set_bridge_log_level(pd_bridge_log_level_t level); // 设置Bridge日志级别
        void force_refresh_bridge_status(void);      // 强制刷新Bridge状态
        
        // v1.2修复：监听模式下的电源信息获取
        String get_bridge_power_info_string(void);   // 获取电源信息的字符串描述
        uint32_t get_bridge_max_power(void);         // 获取最大功率（mW）
        uint16_t get_bridge_voltage_range_min(void); // 获取最小电压（mV）
        uint16_t get_bridge_voltage_range_max(void); // 获取最大电压（mV）
        uint16_t get_bridge_current_limit(void);     // 获取电流限制（mA）
        bool is_bridge_pps_capable(void);            // 检查是否支持PPS
        
        // Bridge功能方法
        // 注意：Bridge模式为纯监听模式，不会进行任何PD握手
        // 只接收和解析PD消息，不会发送任何响应或触发PD协议
        void init_Bridge(uint8_t int_pin);
        void run_Bridge(void);
        int status_bridge_log_readline(char *buffer, int maxlen);
        // Set
        bool set_PPS(uint16_t PPS_voltage, uint8_t PPS_current);
        void set_power_option(enum PD_power_option_t power_option);
        // Clock
        static void clock_prescale_set(uint8_t prescaler);

    protected:
        static FUSB302_ret_t FUSB302_i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t count);
        static FUSB302_ret_t FUSB302_i2c_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t count);
        static FUSB302_ret_t FUSB302_delay_ms(uint32_t t);
        void handle_protocol_event(PD_protocol_event_t events);
        void handle_FUSB302_event(FUSB302_event_t events);
        bool timer(void);
        void set_default_power(void);
        // Device
        FUSB302_dev_t FUSB302;
        PD_protocol_t protocol;
        uint8_t int_pin;
        // Power ready power
        uint16_t ready_voltage;
        uint16_t ready_current;
        // PPS setup
        uint16_t PPS_voltage_next;
        uint8_t PPS_current_next;
        // Status
        virtual void status_power_ready(status_power_t status, uint16_t voltage, uint16_t current);
        uint8_t status_initialized;
        uint8_t status_src_cap_received;
        status_power_t status_power;
        // Timer and counter for PD Policy
        uint16_t time_polling;
        uint16_t time_wait_src_cap;
        uint16_t time_wait_ps_rdy;
        uint16_t time_PPS_request;
        uint8_t get_src_cap_retry_count;
        uint8_t wait_src_cap;
        uint8_t wait_ps_rdy;
        uint8_t send_request;
        static uint8_t clock_prescaler;
        // Time functions        
        void delay_ms(uint16_t ms);
        uint16_t clock_ms(void);
        // Status logging
        virtual void status_log_event(uint8_t status, uint32_t * obj = 0) {}
        // v1.2修复：监听功能数据
        pd_monitor_t pd_monitor;
        virtual void update_monitor_info(void);
        
    // v1.2修复：Bridge功能数据
    bool bridge_mode_enabled;
    char bridge_log_buffer[256];
    uint16_t bridge_log_index;
    pd_bridge_log_level_t bridge_log_level;  // Bridge日志级别
    
    // Log专用Bridge功能数据
    char bridge_status_buffer[128];
};


///////////////////////////////////////////////////////////////////////////////////////////////////
// Optional: PD_UFP_Log_c, extended from PD_UFP_c to provide logging function.
//           Asynchronous, minimal impact on PD timing.
///////////////////////////////////////////////////////////////////////////////////////////////////
struct status_log_t {
    uint16_t time;
    uint16_t msg_header;
    uint8_t obj_count;
    uint8_t status;
};

enum pd_log_level_t {
    PD_LOG_LEVEL_INFO,
    PD_LOG_LEVEL_VERBOSE
};

class PD_UFP_Log_c : public PD_UFP_c
{
    public:
        PD_UFP_Log_c(pd_log_level_t log_level = PD_LOG_LEVEL_INFO, 
                     pd_bridge_log_level_t bridge_log_level = (pd_bridge_log_level_t)1); // 1 = DETAILED
        // Task
        //void print_status(Serial_ & serial);
        void print_status(HardwareSerial & serial);
        // Get
        int status_log_readline(char * buffer, int maxlen);
        
        // Bridge功能方法（继承并重载）
        void init_Bridge(uint8_t int_pin);
        void run_Bridge(void);
        int status_bridge_log_readline(char *buffer, int maxlen);

    protected:
        int status_log_readline_msg(char * buffer, int maxlen, status_log_t * log);
        int status_log_readline_src_cap(char * buffer, int maxlen);
        // Status log functions
        uint8_t status_log_obj_add(uint16_t header, uint32_t * obj);
        virtual void status_log_event(uint8_t status, uint32_t * obj);
        // status log event queue
        status_log_t status_log[16];    // array size must be power of 2 and <=256
        uint8_t status_log_read;
        uint8_t status_log_write;
        // status log object queue
        uint32_t status_log_obj[16];    // array size must be power of 2 and <=256
        uint8_t status_log_obj_read;
        uint8_t status_log_obj_write;
        // state variables
        pd_log_level_t status_log_level;
        pd_bridge_log_level_t bridge_log_level;
        uint8_t status_log_counter;        
        char status_log_time[8];
};

#endif