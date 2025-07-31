#include "HAL.h"
#include "Config.h"

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

    button1.setLongClickHandler(Key1LongClick);
    button2.setLongClickHandler(Key2LongClick);
    button3.setLongClickHandler(Key3LongClick);
    button4.setLongClickHandler(Key4LongClick);
}

// 按键检测
// 按键1: 确认
void Key1LongClick(Button2&btn1){
    unsigned int time1 = btn1.wasPressedFor();
    unsigned int time1max = 1000;

    if (time1 >= time1max) {
        Now_App = 1; // 返回主界面
    }else if (time1 >= 300 && time1 < time1max) {
        bottunStatus = "Button1 Pressed";
    }else if (time1 >= time1max)
    {
        bottunStatus = "Button1 Short Pressed";
    }  
}

// 按键2: 返回
void Key2LongClick(Button2&btn2){
    unsigned int time2 = btn2.wasPressedFor();
    unsigned int time2max = 1000;
    if (time2 >= time2max) {
        bottunStatus = "Button2 Long Pressed";
    }else if (time2 >= 300 && time2 < time2max) {
        bottunStatus = "Button2 Pressed";
    }else if (time2 < 300) {
        bottunStatus = "Button2 Short Pressed";
    }
}

// 按键3: 向下
void Key3LongClick(Button2&btn3){
    unsigned int time3 = btn3.wasPressedFor();
    unsigned int time3max = 1000;
    if (time3 >= time3max) {
        bottunStatus = "Button3 Long Pressed";
        Now_App = 12; // 进入OTA界面
    }else if (time3 >= 300 && time3 < time3max) {
        bottunStatus = "Button3 Pressed";
    }else if (time3 < 300) {
        bottunStatus = "Button3 Short Pressed";
    }
}

// 按键4: 向上
void Key4LongClick(Button2&btn4){
    unsigned int time4 = btn4.wasPressedFor();
    unsigned int time4max = 1000;
    if (time4 >= time4max) {
        bottunStatus = "Button4 Long Pressed";
    }else if (time4 >= 300 && time4 < time4max) {
        bottunStatus = "Button4 Pressed";
    }else if (time4 < 300) {
        bottunStatus = "Button4 Short Pressed";
    }
}

void HAL::Button_Run() {
    button1.loop();
    button2.loop();
    button3.loop();
    button4.loop();
}