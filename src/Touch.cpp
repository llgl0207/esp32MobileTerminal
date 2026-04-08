#include "Touch.h"

int touchX = -1;
int touchY = -1;
int lastTouchX = -1;
int lastTouchY = -1;
int deltaTouchX = 0;
int deltaTouchY = 0;

// 函数功能: 采集四线电阻屏触摸坐标，并输出当前点与位移增量。
// 输入参数: 无（依赖Touch.h中的引脚常量和标定参数）。
// 输出参数: 无返回值，写入touchX/touchY/deltaTouchX/deltaTouchY。
// 算法说明:
// 1) X坐标采样: 在Y+与Y-之间施加电压，读取X-电压并线性映射。
// 2) Y坐标采样: 在X+与X-之间施加电压，读取Y-电压并线性映射。
// 3) 若超出标定范围则判定为未触摸(-1)，最后计算相对位移供拖拽控件使用。
void getTouch(){
        // 语句组功能: 配置为“测X”电路拓扑。
        pinMode(TFT_TOUCH_YN, OUTPUT);
        pinMode(TFT_TOUCH_YP, OUTPUT);
        pinMode(TFT_TOUCH_XN, INPUT_PULLDOWN);
        pinMode(TFT_TOUCH_XP, INPUT_PULLDOWN);
        digitalWrite(TFT_TOUCH_YN, LOW);
        digitalWrite(TFT_TOUCH_YP, HIGH);

        // 语句组功能: 读取X轴并执行阈值判定与坐标映射。
        if(analogRead(TFT_TOUCH_XN)<TFT_TOUCH_MINX||analogRead(TFT_TOUCH_XN)>TFT_TOUCH_MAXX)touchX = -1;
        else{
            touchX = Xlength*(analogRead(TFT_TOUCH_XN)-TFT_TOUCH_MINX)/(float)(TFT_TOUCH_MAXX-TFT_TOUCH_MINX);
            if(InvertX) touchX = Xlength - touchX;
        }
        if(rawOut) touchX = analogRead(TFT_TOUCH_XN);

        // 语句组功能: 切换为“测Y”电路拓扑。
        pinMode(TFT_TOUCH_XN, OUTPUT);
        pinMode(TFT_TOUCH_XP, OUTPUT);
        pinMode(TFT_TOUCH_YN, INPUT_PULLDOWN);
        pinMode(TFT_TOUCH_YP, INPUT_PULLDOWN);
        digitalWrite(TFT_TOUCH_XN, LOW);
        digitalWrite(TFT_TOUCH_XP, HIGH);

        // 语句组功能: 读取Y轴并执行阈值判定与坐标映射。
        if(analogRead(TFT_TOUCH_YN)<TFT_TOUCH_MINY||analogRead(TFT_TOUCH_YN)>TFT_TOUCH_MAXY)touchY = -1;
        else{
            touchY = Ylength*(analogRead(TFT_TOUCH_YN)-TFT_TOUCH_MINY)/(float)(TFT_TOUCH_MAXY-TFT_TOUCH_MINY);
            if(InvertY) touchY = Ylength - touchY;
        }
        if(rawOut) touchY = analogRead(TFT_TOUCH_YN);

        // 计算触摸坐标的变化量
        deltaTouchX = touchX - lastTouchX;
        deltaTouchY = touchY - lastTouchY;
        lastTouchX = touchX;
        lastTouchY = touchY;
    }
