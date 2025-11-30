/*
 * PD_UFP Bridge模式完整使用示例
 * 
 * 这个示例展示如何使用新添加的Bridge功能和所有监听函数
 * FUSB302芯片作为中间设备，不参与PD握手
 * 
 * 拓扑图：
 * USB(SRC)-VBUS-Device(SNK)
 *     CC1-FUSB302-Device
 *     CC2-FUSB302-Device  
 *     GND-FUSB302-Device
 * 
 * 重要说明：
 * - get_ps_status() 现在可以在Bridge模式下正常工作
 * - Bridge模式已修复PD协议栈初始化问题
 * - 可以监听但不参与PD电源协商过程
 */

#include "PD_UFP.h"  // 确保包含修改后的头文件
#define FUSB302_INT_PIN 2  // FUSB302中断引脚连接到GPIO2

PD_UFP_Log_c PD_UFP(PD_LOG_LEVEL_VERBOSE);

char pdbuf[256];
uint8_t PD_Src_Cap_Count = 0;  // PD源能力数量
uint8_t PD_Position = 0;       // PD位置
uint8_t ccbus_used = 0;        // CC线使用状态
uint32_t total_crc_count = 0;  // 总CRC计数
uint32_t total_reject_count = 0; // 总拒绝计数
bool cc_connected = false;     // CC连接状态

void setup() {
    Serial.begin(115200);
    
    // 初始化Bridge模式
    PD_UFP.init_Bridge(FUSB302_INT_PIN);
    PD_UFP.enable_vbus_sense(0); // 关闭VBUS检测，FUSB302作为监听设备
    
    Serial.println("FUSB302 PD Bridge Init Complete!");
    Serial.println("Monitoring PD communication between Source and Sink...");
    Serial.println("==================================================");
}

void loop() {
    // 运行Bridge模式监听
    PD_UFP.run_Bridge();
    
    static long PDtimeMillis = millis(); 
    if (millis() - PDtimeMillis >= 500) { // 500ms间隔
        PDtimeMillis = millis();
        
        // 读取监听日志
        int len = PD_UFP.status_bridge_log_readline(pdbuf, sizeof(pdbuf) - 1);
        if (len > 0) {
            Serial.print(pdbuf);
        }
        
        // 获取所有Bridge监听数据
        float voltage = PD_UFP.get_bridge_voltage();
        float current = PD_UFP.get_bridge_current();
        String powerMode = PD_UFP.get_bridge_power_mode();
        int packetCount = PD_UFP.get_bridge_packet_count();
        
        // 获取新的Bridge监听数据
        PD_Src_Cap_Count = PD_UFP.get_bridge_src_cap_count();      // 源能力计数
        PD_Position = PD_UFP.get_bridge_selected_position();       // PD位置
        ccbus_used = PD_UFP.get_bridge_cc_pin();                   // CC线状态：0/NULL 1/CC1 2/CC2
        total_crc_count = PD_UFP.get_bridge_good_crc_count();      // 成功CRC计数
        total_reject_count = PD_UFP.get_bridge_reject_count();     // 拒绝计数
        cc_connected = PD_UFP.get_bridge_cc_status();              // CC连接状态
        
        status_power_t ps_status = PD_UFP.get_ps_status();
        
        // 显示完整的监听信息
        Serial.println("=== Bridge Monitor Status ===");
        Serial.printf("Power: %.2fV @ %.2fA [%s]", voltage, current, powerMode.c_str());
        
        // 根据电源状态显示详细信息
        switch(ps_status) {
            case STATUS_POWER_TYP:
                Serial.println(" [固定电压模式]");
                break;
            case STATUS_POWER_PPS:
                Serial.println(" [PPS可编程模式]");
                break;
            case STATUS_POWER_NA:
                Serial.println(" [无电源或未协商]");
                break;
        }
        
        Serial.printf("PD Packets: %d, CRC OK: %lu, Rejected: %lu\r\n", 
                     packetCount, total_crc_count, total_reject_count);
        Serial.printf("Source Capabilities: %d entries\r\n", PD_Src_Cap_Count);
        Serial.printf("Selected Position: %d\r\n", PD_Position);
        Serial.printf("CC Status: Connected=%s, Pin=", cc_connected ? "YES" : "NO");
        
        // CC引脚状态显示
        switch(ccbus_used) {
            case 0: Serial.println("NULL"); break;
            case 1: Serial.println("CC1"); break;
            case 2: Serial.println("CC2"); break;
            default: Serial.println("Unknown"); break;
        }
        
        Serial.println("======================================");
        
        // 示例：重置监听计数器（可选功能）
        // PD_UFP.reset_bridge_monitor();
    }
}

/*
 * 完整监听功能说明：
 * 
 * === 基础监听函数 ===
 * 1. get_bridge_voltage() - 返回float类型的实际电压值（V）
 *    - FIX模式：返回固定电压值（50mV单位转换）
 *    - PPS模式：返回可编程电压值（20mV单位转换）
 *    - 无电源时：返回0.0V
 * 
 * 2. get_bridge_current() - 返回float类型的实际电流值（A）
 *    - FIX模式：返回固定电流值（10mA单位转换）
 *    - PPS模式：返回可编程电流值（50mA单位转换）
 *    - 无电源时：返回0.0A
 * 
 * 3. get_bridge_power_mode() - 返回String类型的电源模式
 *    - "FIX"：固定电压模式
 *    - "PPS"：可编程电源模式
 *    - "N/A"：无电源
 * 
 * 4. get_bridge_packet_count() - 返回int类型的PD包数量
 *    - 统计监听到的PD数据包总数
 * 
 * === 重要：get_ps_status() 在Bridge模式下的使用 ===
 * 
 * 5. get_ps_status() - 返回status_power_t类型的电源状态
 *    - STATUS_POWER_TYP：固定电压模式（5V/9V/15V/20V等）
 *    - STATUS_POWER_PPS：可编程电源模式
 *    - STATUS_POWER_NA：无电源或未完成协商
 *    - **已修复**：Bridge模式下现在可以正常获取电源状态
 *    - 用法：status_power_t ps = PD_UFP.get_ps_status();
 *    - 配合switch语句可以根据电源状态执行不同逻辑
 * 
 * === 新增高级监听函数 ===
 * 6. get_bridge_src_cap_count() - 返回uint8_t类型的源能力数量
 *    - 获取PD源设备支持的能力选项数量
 * 
 * 7. get_bridge_selected_position() - 返回uint8_t类型的PD位置
 *    - 获取当前选择的电源配置位置
 * 
 * 8. get_bridge_cc_pin() - 返回uint8_t类型的CC线状态
 *    - 0：NULL（无连接）
 *    - 1：CC1线连接
 *    - 2：CC2线连接
 * 
 * 9. get_bridge_good_crc_count() - 返回uint32_t类型的成功CRC计数
 *    - 统计校验成功的PD数据包数量
 * 
 * 10. get_bridge_reject_count() - 返回uint32_t类型的拒绝计数
 *     - 统计被拒绝的PD请求数量
 * 
 * 11. get_bridge_cc_status() - 返回bool类型的CC连接状态
 *     - true：CC线已连接
 *     - false：CC线未连接
 * 
 * 12. reset_bridge_monitor() - void类型，重置监听数据
 *     - 重置所有计数器，保留当前电压电流值
 * 
 * 13. status_bridge_log_readline() - 读取格式化日志信息
 *     - 返回当前状态的字符串格式信息
 *     - 包含详细的PD通信状态
 * 
 * === Bridge模式特点 ===
 * - FUSB302芯片不参与PD握手，只作为监听设备
 * - 实时监听USB-C接口的PD通信
 * - 不影响原有的PD功能
 * - 适用于PD通信分析、调试和监控
 * - 提供完整的PD协议栈信息
 * - 支持PD2.0和PD3.0协议
 * - **已修复**：get_ps_status()可以正常获取电源状态
 * 
 * === 使用注意事项 ===
 * - Bridge模式下FUSB302应连接到CC1/CC2和GND线
 * - 不要在Bridge模式下启用VBUS检测
 * - 监听数据会在PD通信过程中实时更新
 * - 初始化时会自动添加设备状态日志
 * - 支持所有原有的UFP功能
 * - get_ps_status()现在可以在Bridge模式下正常返回值
 */