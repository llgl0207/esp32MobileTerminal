#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "UI.h"
#include "blockyProg.h"


uiSlider *sliderPtr;
void setup() {
    Serial.begin(115200);

    // 1. 初始化 TFT
    tft.init();
    tft.setRotation(1);
    tft.invertDisplay(false); // 修复 ST7789 屏幕反色问题
    tft.fillScreen(TFT_WHITE);
    tft.drawString("Hello, World!", 100, 10);
    createActivity("Main");
    blockyProgInit();
    controlActivityPtr = getActivity("blockyProgInstance");
    renderActivityPtr = getActivity("blockyProgInstance");
    uiRender();
}

void loop() {
    getTouch();
    btnMgr();
    //uiRender();
    tft.drawPixel(touchX, touchY, TFT_RED);
    /* Serial.print(touchX);
    Serial.print(",");
    Serial.println(touchY); */
    delay(5);           // 让出一点时间
}