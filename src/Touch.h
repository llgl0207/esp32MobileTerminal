#include <Arduino.h>

#define InvertX 0 // 1表示对X坐标取反，适配触摸排线方向
#define InvertY 1 // 1表示对Y坐标取反，适配触摸排线方向
#define rawOut 0  // 1表示输出原始ADC值，0表示输出映射后的屏幕坐标

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

    // ======= 触摸屏状态量 =======
    extern int touchX, touchY; // 当前触摸点坐标，单位为像素；未触摸时为-1
    extern int lastTouchX, lastTouchY; // 上一次的触摸坐标
    extern int deltaTouchX, deltaTouchY; // 当前与上次触摸坐标差值，常用于拖拽位移
    extern long touchTime; // 触摸持续时间，单位毫秒

    #define Xlength 320
    #define Ylength 240

    // 读取电阻屏触摸坐标。
    // 输入: 无（使用固定触摸引脚和标定参数）。
    // 输出: 无返回值，结果写入touchX/touchY及deltaTouchX/deltaTouchY。
    // 算法: 先在X轴施加电压读取Y方向电位，再在Y轴施加电压读取X方向电位，
    //      将ADC原始值按标定区间线性映射到屏幕像素坐标。
    void getTouch();
#endif