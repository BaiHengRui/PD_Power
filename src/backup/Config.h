#ifndef CONFIG_H
#define CONFIG_H
// Build Version
// >Major Minor Patch (Language)
#define FirmwareVer "1.1.0b"

#define I2C_SDA_PIN 36
#define I2C_SCL_PIN 35

#define FUSB302_INT_PIN 38
#define LCD_BL_PIN 9
#define BUZZER_PIN 33 
#define VBUS_ADC 7
#define DN_PIN 3
#define DP_PIN 4
#define CC1_PIN 1
#define CC2_PIN 2
#define NTC_PIN 17
#define SW1 0
#define SW2 8
#define SW3 9
#define SW4 16

#define BUZZER_FREQUENCY 4000 // Buzzer frequency in Hz

#endif

//还在开发，代码未经过完整测试，请勿用于商业用途