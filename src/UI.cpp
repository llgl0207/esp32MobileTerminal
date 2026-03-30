#include "UI.h"

TFT_eSPI tft = TFT_eSPI(); // 创建 TFT 对象
std::vector<uiElementBase*> uiElementsPool;
std::vector<uiButtonBase*> uiButtonsPool;

uint32_t backgoundColor = TFT_WHITE;
void nullFunc(){
    //Need to be empty.
}
void uiRender(){
    tft.fillScreen(backgoundColor);
    //Serial.println("Drawing element...");
    for(auto element : uiElementsPool){
        element->draw(); // 调用每个元素的 draw 方法进行绘制
    }
}
void lowRender(){
    static uint16_t renderTimer = 0;
    if(renderTimer<10){
        renderTimer++;
        return;
    }
    renderTimer=0;
    
    tft.fillScreen(backgoundColor);
    //Serial.println("Drawing element...");
    for(auto element : uiElementsPool){
        element->draw(); // 调用每个元素的 draw 方法进行绘制
    }
}
void btnMgr(){
    for(int i = uiButtonsPool.size() - 1; i >= 0; i--){
        if(uiButtonsPool[i]->touchResponse()){
            break; // 如果有按钮响应了触摸事件，停止检查其他按钮
        }
    }
}
