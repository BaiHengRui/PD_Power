使用ESP32-S3主控，INA226检测芯片，FUSB302 PD芯片的小型可调电源。可选的PD/QC功能（需要充电器支持）

## 未开放完全阶段
## 如果你想要测试PD功能 到"src/hal/HAL_Protocol.cpp"文件里，`PD_UFP.init_PPS(FUSB302_INT_PIN,PPS_V(20),PPS_A(5));在`void HAL::PD_Init()`函数里
`PD_UFP.init_PPS(FUSB302_INT_PIN,PPS_V(20),PPS_A(5));`则是握手PPS档20V 5A
`PD_UFP.init_PPS(FUSB302_INT_PIN,PPS_V(PD_POWER_OPTION_MAX_VOLTAGE), PPS_A(PD_POWER_OPTION_MAX_CURRENT), PD_POWER_OPTION_MAX_POWER);`则是握手获取到的充电器最大的挡位
`PD_UFP.init(FUSB302_INT_PIN,PD_POWER_OPTION_MAX_9V);`是握手充电器固定档 9V挡位
## 只能开启其中一个！
