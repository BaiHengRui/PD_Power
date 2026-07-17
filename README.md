# PD_Power — ESP32-S3 PD 快充电源监视器

基于 ESP32-S3 + INA226 + FUSB302，支持 PD/QC 协议监听、电压电流曲线、OTA 升级。

> 硬件详见 [立创开源平台](https://oshwhub.com/bhr13151022)

## 硬件配置

| 组件 | 型号 | 接口 |
|------|------|------|
| 主控 | ESP32-S3 | SPI/I2C |
| 电量计 | INA226 | I2C (5mΩ 采样) |
| PD 芯片 | FUSB302 | I2C + INT |
| 屏幕 | ST7789 240×240 | SPI |
| WiFi | ESP32-S3 内置 | OTA 升级 |

## 项目结构

```
PD_Power/
├── platformio.ini           # PlatformIO 项目配置
├── src/
│   ├── main.cpp             # FreeRTOS 任务入口 (6 个任务)
│   ├── hal/                 # 硬件抽象层
│   │   ├── hal.h/cpp        # 系统初始化 + 曲线更新 + Toast
│   │   ├── globals.h/cpp    # 全局变量
│   │   ├── hal_button.cpp   # 按键 (Button2: 单击/长按)
│   │   ├── hal_buzzer.cpp   # 蜂鸣器
│   │   ├── hal_gpio.cpp     # ADC 读取
│   │   ├── hal_ina.cpp      # INA226 驱动
│   │   ├── hal_lcd.cpp      # LCD 初始化/亮度/旋转
│   │   ├── hal_nvs.cpp      # NVS 持久化
│   │   ├── hal_pd.cpp       # PD Sniffer (PDLib)
│   │   └── hal_web.cpp      # WiFi + OTA WebServer
│   ├── ui/                  # UI 界面
│   │   ├── ui.h
│   │   └── ui.cpp           # 主页/曲线/PD日志/系统信息/OTA 等
│   ├── assets/              # 资源
│   │   ├── fonts/           # 字体 (MiSans, OPPOSans, Din1451)
│   │   └── imgs/            # 图标
│   ├── html/                # OTA 网页
│   └── backup/              # 旧架构备份
├── lib/                     # 第三方库
│   ├── Button2/             # 按钮
│   ├── INA226Lib/           # INA226
│   ├── PDLib/               # PD Sniffer + Parser
│   ├── TFT_eSPI/            # TFT 驱动
│   └── WiFiManager/         # WiFi 配网
└── test/
```

## 功能

- [x] 电压/电流/功率实时测量 (INA226)
- [x] PD 协议被动监听 (Source Cap / Request / PPS / VDM)
- [x] 电压电流曲线 (ring buffer + sticky auto-scale)
- [x] PD 日志解析显示
- [x] OTA 无线升级 (WebServer + WiFiManager)
- [x] 系统信息页
- [ ] QC 协议
- [ ] 菜单设置
- [ ] PD Sink 触发

## 页面与按键

| 按键 | 功能 |
|------|------|
| SW1 (Btn1) | 下一页 |
| SW2 (Btn2) | 上一页 |
| SW3 (Btn3) | 一键进入 OTA |
| SW4 (Btn4) | 回主页 |

| 页码 | 页面 |
|------|------|
| 0 | 主界面 (V/A/W + PDO) |
| 1 | 电压电流曲线 |
| 2 | 按键测试 |
| 3 | 菜单 (待实现) |
| 4 | 设置 |
| 5 | 系统信息 |
| 6 | PD 电源配置 |
| 7 | QC 快充 |
| 8 | PD 日志监听 |
| 9-13 | WiFi/OTA 相关 |

## FreeRTOS 任务

| 任务 | 核心 | 栈 | 频率 | 职责 |
|------|------|-----|------|------|
| Sensor | 0 | 3K | 200Hz | INA226 + GPIO |
| PD | 0 | 4K | 100Hz | PD Sniffer |
| Button | 0 | 3K | 100Hz | 按键扫描 |
| Graph | 0 | 2K | 50Hz | 曲线 ring buffer |
| WiFi | 1 | 8K | 100Hz | WiFiManager + OTA |
| Display | 0 | 8K | 25Hz | UI 渲染 |

## 烧录

```bash
pio run --target upload
pio device monitor -b 115200
```
![img v1.3](/imgs/v12-2.jpg)