#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "UI.h"
#include "blockyProg.h"
#include "BCP.h"


uiSlider *sliderPtr;

void uiRun(void *pvParameters){
    while(1){
        getTouch();
        btnMgr();
        //uiRender();
        tft.drawPixel(touchX, touchY, TFT_RED);
        /* Serial.print(touchX);
        Serial.print(",");
        Serial.println(touchY); */
        vTaskDelay(5);           // 让出一点时间
    }
}
void setup() {
    Serial.begin(115200);
    //BCP初始化
    
    //初始化 TFT
    tft.init();
    tft.setRotation(1);
    tft.invertDisplay(false); // 修复 ST7789 屏幕反色问题
    tft.fillScreen(TFT_WHITE);
    tft.drawString("Hello, World!", 100, 10);
    createActivity("Main");
    blockyProgInit();
    controlActivityPtr = getActivity("blockyProgMain");
    renderActivityPtr = getActivity("blockyProgMain");
    uiRender();
    //xTaskCreate(uiRun, "uiRun", 20480, NULL, 1, NULL);
}

void loop() {
    //BCP配对刷新
    static uint32_t lastPairTryMs = 0;
	if (millis() - lastPairTryMs > 2000) {
		pairDevice(targetMac);
		lastPairTryMs = millis();
	}

    getTouch();
    btnMgr();
    //uiRender();
    tft.drawPixel(touchX, touchY, TFT_RED);
    /* Serial.print(touchX);
    Serial.print(",");
    Serial.println(touchY); */
    delay(5);           // 让出一点时间
}