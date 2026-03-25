#include "UI.h"

TFT_eSPI tft = TFT_eSPI(); // 创建 TFT 对象
std::vector<uiElementBase*> uiElementsPool;
std::vector<uiButtonBase*> uiButtonsPool;

void uiRender(){
    //Serial.println("Drawing element...");
    getTouch();
    btnMgr();
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
