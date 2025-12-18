使用ESP32-S3主控，INA226检测芯片，FUSB302 PD芯片的小型电源。可选的PD/QC功能（需要充电器支持）

# 未开放完全阶段
## 更改显示界面
到"PD_Power\src\main.cpp"文件里，更改setup()函数，Now_App = 2;的值，界面代号在"src\hal\hal.cpp"里的`Sys_Run()`有注明

## 如果你想要测试PD功能
### 注意！ 只能开启其中一个！
到"src/hal/hal_Protocol.cpp"文件里，`void HAL::PD_Init()`函数  

`PD_UFP.init_PPS(FUSB302_INT_PIN,PPS_V(20),PPS_A(5));`则是握手PPS档20V 5A

`PD_UFP.init_PPS(FUSB302_INT_PIN,PPS_V(PD_POWER_OPTION_MAX_VOLTAGE), PPS_A(PD_POWER_OPTION_MAX_CURRENT), PD_POWER_OPTION_MAX_POWER);`则是握手获取到的充电器最大的挡位  

`PD_UFP.init(FUSB302_INT_PIN,PD_POWER_OPTION_MAX_9V);`是握手充电器固定档 9V挡位  

`PD_UFP.init_Bridge(FUSB302_INT_PIN);`则是开启监听模式。目前不可以和SNK模式同时使用。
监听模式目前有启动顺序较严格的要求！但功能影响不大
注意！监听模式的FIX挡位可以正常解析，PPS挡位解析会出现错误！
[查看效果图片](#pdo数据包解析图)


#### 如果屏幕会显示"闪烁"/"错位"/"颜色错误"等情况
更改lib\TFT_eSPI\User_Setup.h的374行`#define SPI_FREQUENCY` 默认SPI速率为80MHz，可以尝试降低到55MHz后清除工程然后重新编译，但降低SPI速率会使刷新率也降低（应该不会闪烁问题不算大）。
### 目前进度
1. 电压电流测量
2. PD触发
3. PD日志监听获取
4. OTA升级
### 待实现/优化的
1. 按键逻辑
2. 菜单
3. 可自定义选项且保持的PD设置
4. 优化运行性能
5. QC协议功能栈
6. PD协议功能栈
### 遗留问题！
1. PPS数据包解析问题
2. FIX/PPS的PDO数据包解析过慢
3. PDO数据包逻辑需优化，必须在FUSB302启动后握手电源才能正常解析数据，否则显示0/0
4. 无法正常获取解析到的电压电流到变量中，原因未知
5. （好难搞）

# 更新日志 
    
    2025/12/14日
        新增PD数据包显示日志输出，优化日志显示布局，临时解决底部信息栏功率不显示问题
        遗留了PPS解析失败的问题，卡在判断PPS状态上了。
        因为PDO缓存列表，使用需要重新请求约5次刷新列表才能继续解析数据包，待优化。
    2025/12/18日
        新增了TFT_eSPI库圆环绘制



### PDO数据包解析效果图
LCD屏幕为本工程现版本的LOG界面，右侧屏幕为KM003C的PD触发界面，使用酷态科10号电能棒，C-C数据线（带E-Marker芯片/48V5A EPR）
![img1 初代](/imgs/v1-1.jpg)
![img2 初代](/imgs/v1-2.jpg)
### 更新1
![img v1.3](/imgs/v12-1.jpg)
![img v1.3](/imgs/v12-2.jpg)