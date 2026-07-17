// ============================================================
// Button module (Button2 lib)
// ============================================================
#include "hal.h"
#include <Button2.h>

static Button2 btn1, btn2, btn3, btn4;

static void onBtn1(Button2& b) { nowApp = (nowApp + 1) % (maxApp + 1); }
static void onBtn2(Button2& b) { nowApp = (nowApp > 0) ? nowApp - 1 : maxApp; }
static void onBtn3(Button2& b) { nowApp = AppState::WIFI_CONNECT; }  // 短按进入 OTA 流程
static void onBtn4(Button2& b) { nowApp = AppState::MAIN; }          // 回主页

void HAL::Button_Init()
{
    btn1.begin(SW1, INPUT_PULLUP);
    btn2.begin(SW2, INPUT_PULLUP);
    btn3.begin(SW3, INPUT_PULLUP);
    btn4.begin(SW4, INPUT_PULLUP);

    btn1.setClickHandler(onBtn1);
    btn2.setClickHandler(onBtn2);
    btn3.setLongClickHandler(onBtn3);  // 长按触发 OTA
    btn4.setClickHandler(onBtn4);
}

void HAL::Button_Loop()
{
    btn1.loop();
    btn2.loop();
    btn3.loop();
    btn4.loop();
}
