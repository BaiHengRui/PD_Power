使用ESP32-S3主控，INA226检测芯片，FUSB302 PD芯片的小型可调电源。可选的PD/QC功能（需要充电器支持）

# 未开放完全阶段
## 更改显示界面
到"PD_Power\src\main.cpp"文件里，更改setup()函数，Now_App = 2;的值，界面代号在"src\hal\HAL.cpp"里的`Sys_Run()`有注明

## 如果你想要测试PD功能
到"src/hal/HAL_Protocol.cpp"文件里，`void HAL::PD_Init()`函数  

`PD_UFP.init_PPS(FUSB302_INT_PIN,PPS_V(20),PPS_A(5));`则是握手PPS档20V 5A

`PD_UFP.init_PPS(FUSB302_INT_PIN,PPS_V(PD_POWER_OPTION_MAX_VOLTAGE), PPS_A(PD_POWER_OPTION_MAX_CURRENT), PD_POWER_OPTION_MAX_POWER);`则是握手获取到的充电器最大的挡位  

`PD_UFP.init(FUSB302_INT_PIN,PD_POWER_OPTION_MAX_9V);`是握手充电器固定档 9V挡位  

## 只能开启其中一个！

### 目前进度
1. 电压电流测量
2. 简易QC协议识别
3. PD触发
4. PD日志获取
5. OTA升级
### 待实现的
1. 按键逻辑
2. 设置菜单
3. 可保持的PD设置
