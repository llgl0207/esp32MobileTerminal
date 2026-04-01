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
uiButton* backBtn;
uiButton* closeBtn;
void closePopup(){
    delete backBtn;
    delete closeBtn;
}

void popUpInverseColor(){
    backBtn->inverseColor();
}
void popUp(char* text){
    #define sideLength 10//定义popUp方框的边长
    backBtn = new uiButton(text,sideLength,sideLength,popUpInverseColor,320-2*sideLength,240-2*sideLength,0xFFFF);
    backBtn->startPressCallback = popUpInverseColor;//神秘屎山，这么写只是为了提供一个按钮遮罩，让弹窗界面覆盖在所有按钮之前，阻止下方按钮被点击，并且弹窗界面被按下不变色
    backBtn->endLongPressCallback = popUpInverseColor;
    closeBtn = new uiButton("X",320-25-sideLength,sideLength,closePopup,25,25,TFT_LIGHTGREY);
}

