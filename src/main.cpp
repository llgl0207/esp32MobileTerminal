#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "UI.h"

void setup() {
    Serial.begin(115200);

    // 1. 初始化 TFT
    tft.init();
    tft.setRotation(1);
    tft.invertDisplay(false); // 修复 ST7789 屏幕反色问题
    
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Hello, World!", 100, 10);
    new uiButton("C", 100, 100); // 创建一个按钮实例
}

void loop() {
    tft.drawPixel(touchX, touchY, TFT_RED);
    /* Serial.print(touchX);
    Serial.print(",");
    Serial.println(touchY); */
    uiRender();
    //delay(5);           // 让出一点时间
}
