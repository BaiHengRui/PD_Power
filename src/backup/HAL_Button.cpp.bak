#include "hal.h"
#include "Config.h"
#include <Button2.h>

// 条件编译：根据是否启用LVGL选择按键处理函数
#ifdef ENABLE_LVGL
// LVGL版本的按键处理函数声明
void Key1LongClick_LVGL(Button2&btn1);
void Key2LongClick_LVGL(Button2&btn2);
void Key3LongClick_LVGL(Button2&btn3);
void Key4LongClick_LVGL(Button2&btn4);
#endif

void Key1LongClick(Button2&btn1);
void Key2LongClick(Button2&btn2);
void Key3LongClick(Button2&btn3);
void Key4LongClick(Button2&btn4);

Button2 button1;
Button2 button2;
Button2 button3;
Button2 button4;

void HAL::Button_Init() {
    button1.begin(SW1);
    button2.begin(SW2);
    button3.begin(SW3);
    button4.begin(SW4);

#ifdef ENABLE_LVGL
    // LVGL版本：使用LVGL按键处理函数
    button1.setLongClickHandler(Key1LongClick_LVGL);
    button2.setLongClickHandler(Key2LongClick_LVGL);
    button3.setLongClickHandler(Key3LongClick_LVGL);
    button4.setLongClickHandler(Key4LongClick_LVGL);
    
    Serial.println("Buttons initialized for LVGL version");
#else
    // 原版本：使用原有按键处理函数
    button1.setLongClickHandler(Key1LongClick);
    button2.setLongClickHandler(Key2LongClick);
    button3.setLongClickHandler(Key3LongClick);
    button4.setLongClickHandler(Key4LongClick);
    
    Serial.println("Buttons initialized for original version");
#endif
}

// 按键检测 - 原版本处理
// 按键1: 确认
void Key1LongClick(Button2&btn1){
    unsigned int time1 = btn1.wasPressedFor();
    unsigned int time1max = 1000;

    if (time1 >= time1max) {
        Now_App = 1; // 返回主界面
        bottonStatus = "Button1 Long: Main";
    }else if (time1 >= 300 && time1 < time1max) {
        bottonStatus = "Button1 Pressed";
    }
}

// 按键2: 返回
void Key2LongClick(Button2&btn2){
    unsigned int time2 = btn2.wasPressedFor();
    unsigned int time2max = 1000;
    if (time2 >= time2max) {
        bottonStatus = "Button2 Long Pressed";
    }else if (time2 >= 300 && time2 < time2max) {
        bottonStatus = "Button2 Pressed";
    }
}

// 按键3: 向下
void Key3LongClick(Button2&btn3){
    unsigned int time3 = btn3.wasPressedFor();
    unsigned int time3max = 1000;
    if (time3 >= time3max) {
        bottonStatus = "Button3 Long Pressed";
        Now_App = 12; // 进入OTA界面
    }else if (time3 >= 300 && time3 < time3max) {
        bottonStatus = "Button3 Pressed";
    }
}

// 按键4: 向上
void Key4LongClick(Button2&btn4){
    unsigned int time4 = btn4.wasPressedFor();
    unsigned int time4max = 1000;
    if (time4 >= time4max) {
        bottonStatus = "Button4 Long Pressed";
    }else if (time4 >= 300 && time4 < time4max) {
        bottonStatus = "Button4 Pressed";
    }
}

void HAL::Button_Run() {
    button1.loop();
    button2.loop();
    button3.loop();
    button4.loop();
}

#ifdef ENABLE_LVGL
// LVGL版本的按键处理函数实现
void Key1LongClick_LVGL(Button2&btn1) {
    unsigned int time1 = btn1.wasPressedFor();
    unsigned int time1max = 1000;

    if (time1 >= time1max) {
        // 长按：返回主界面
        HAL::show_screen(AppState::Main);
        bottonStatus = "Button1 Long: Main";
    } else if (time1 >= 300 && time1 < time1max) {
        // 中等按压：菜单确认
        HAL::confirm_menu_selection();
        bottonStatus = "Button1: Confirm";
    }
}

void Key2LongClick_LVGL(Button2&btn2) {
    unsigned int time2 = btn2.wasPressedFor();
    unsigned int time2max = 1000;
    if (time2 >= time2max) {
        // 长按：返回主界面
        HAL::show_screen(AppState::Main);
        bottonStatus = "Button2 Long: Main";
    } else if (time2 >= 300 && time2 < time2max) {
        // 中等按压：返回上级
        HAL::navigate_back();
        bottonStatus = "Button2: Back";
    }
}

void Key3LongClick_LVGL(Button2&btn3) {
    unsigned int time3 = btn3.wasPressedFor();
    unsigned int time3max = 1000;
    if (time3 >= time3max) {
        // 长按：直接进入OTA界面
        HAL::show_screen(AppState::OTA_Update);
        bottonStatus = "Button3 Long: OTA";
    } else if (time3 >= 300 && time3 < time1max) {
        // 中等按压：向下导航
        HAL::navigate_down();
        bottonStatus = "Button3: Down";
    }
}

void Key4LongClick_LVGL(Button2&btn4) {
    unsigned int time4 = btn4.wasPressedFor();
    unsigned int time4max = 1000;
    if (time4 >= time4max) {
        // 长按：打开菜单
        HAL::show_screen(AppState::Menu);
        bottonStatus = "Button4 Long: Menu";
    } else if (time4 >= 300 && time4 < time1max) {
        // 中等按压：向上导航
        HAL::navigate_up();
        bottonStatus = "Button4: Up";
    }
}
#endif