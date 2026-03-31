#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "UI.h"
void btnCallback();

uiSlider *sliderPtr;
void setup() {
    Serial.begin(115200);

    // 1. 初始化 TFT
    tft.init();
    tft.setRotation(1);
    tft.invertDisplay(false); // 修复 ST7789 屏幕反色问题
    tft.fillScreen(TFT_WHITE);
    tft.drawString("Hello, World!", 100, 10);
    new uiButton("C", 100, 100, btnCallback); // 创建一个按钮实例
    new uiDragButton("HaHa", 200, 100); // 创建一个按钮实例
    sliderPtr = new uiSlider("Slider", 25, 215, 270
        
        , 0.5); // 创建一个滑块实例
    uiRender();
}

void loop() {
    getTouch();
    btnMgr();
    tft.drawPixel(touchX, touchY, TFT_RED);
    /* Serial.print(touchX);
    Serial.print(",");
    Serial.println(touchY); */
    delay(5);           // 让出一点时间
}

void btnCallback() {
    sliderPtr->percentage = 1.3;
}