#include <Arduino.h>

#define InvertX 0
#define InvertY 1
#define rawOut 0

#define clickTime 200


#ifndef TOUCH_H
#define TOUCH_H 
    
    // ======= 触摸屏引脚定义 =======
    const uint8_t TFT_TOUCH_XN = 3;  // -X
    const uint8_t TFT_TOUCH_YN = 8;  // -Y
    const uint8_t TFT_TOUCH_XP = 18; // +X
    const uint8_t TFT_TOUCH_YP = 17; // +Y

    // ======= 触摸屏校准参数 =======
    const uint16_t TFT_TOUCH_MINX = 313;
    const uint16_t TFT_TOUCH_MAXX = 3000;
    const uint16_t TFT_TOUCH_MINY = 490;
    const uint16_t TFT_TOUCH_MAXY = 3000;

    // ======= 触摸屏读取函数 =======
    extern int touchX, touchY; // 全局变量存储触摸坐标
    extern long touchTime; // 触摸持续时间，单位毫秒

    #define Xlength 320
    #define Ylength 240

    void getTouch();// 读取触摸坐标并存储在 touchX 和 touchY 中，未触摸时为 -1
#endif