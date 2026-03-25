#include "Touch.h"

int touchX = -1;
int touchY = -1;
int lastTouchX = -1;
int lastTouchY = -1;
int deltaTouchX = 0;
int deltaTouchY = 0;
void getTouch(){
        pinMode(TFT_TOUCH_YN, OUTPUT);
        pinMode(TFT_TOUCH_YP, OUTPUT);
        pinMode(TFT_TOUCH_XN, INPUT_PULLDOWN);
        pinMode(TFT_TOUCH_XP, INPUT_PULLDOWN);
        digitalWrite(TFT_TOUCH_YN, LOW);
        digitalWrite(TFT_TOUCH_YP, HIGH);
        if(analogRead(TFT_TOUCH_XN)<TFT_TOUCH_MINX||analogRead(TFT_TOUCH_XN)>TFT_TOUCH_MAXX)touchX = -1;
        else{
            touchX = Xlength*(analogRead(TFT_TOUCH_XN)-TFT_TOUCH_MINX)/(float)(TFT_TOUCH_MAXX-TFT_TOUCH_MINX);
            if(InvertX) touchX = Xlength - touchX;
        }
        if(rawOut) touchX = analogRead(TFT_TOUCH_XN);
        pinMode(TFT_TOUCH_XN, OUTPUT);
        pinMode(TFT_TOUCH_XP, OUTPUT);
        pinMode(TFT_TOUCH_YN, INPUT_PULLDOWN);
        pinMode(TFT_TOUCH_YP, INPUT_PULLDOWN);
        digitalWrite(TFT_TOUCH_XN, LOW);
        digitalWrite(TFT_TOUCH_XP, HIGH);
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
