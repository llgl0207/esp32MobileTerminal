#include "UI.h"

uiActivity::~uiActivity(){
    // Elements remove themselves from uiElementsPool in their destructors.
    // Delete from back until empty to avoid iterator invalidation.
    while(!uiElementsPool.empty()){
        delete uiElementsPool.back();
    }
}
uiElementBase::~uiElementBase(){
    // 从容器中移除当前对象
    auto it = find(thisActivity->uiElementsPool.begin(), thisActivity->uiElementsPool.end(), this);
    if(it != thisActivity->uiElementsPool.end()){
        thisActivity->uiElementsPool.erase(it);
    }
}

void uiButtonBase::Pressed(){
    lastInTheBtn = nowInTheBtn;
    if(lastPressed){//这是一个持续按压判断，如果按钮从此处按下，即使后续松手时不在按钮上也判断为按下
        if(touchX>=x&&touchX<=x+width&&touchY>=y&&touchY<=y+height){
            nowPressed = true;
            nowInTheBtn = true;
            return;
        }else if(touchX!=-1&&touchY!=-1){
            nowPressed = true;
            nowInTheBtn = false;
            return;
        }else{
            nowPressed = false;
            nowInTheBtn = false;
            return;
        }
    }
    if(touchX==-1||touchY==-1){
        return;
    }

    if(touchX>=x&&touchX<=x+width&&touchY>=y&&touchY<=y+height){
        nowPressed = true;
        nowInTheBtn = true;
    }else{
        nowPressed = false;
        nowInTheBtn = false;
    }
}

bool uiButtonBase::touchResponse(){
    Pressed();
    if(!lastPressed&&!nowPressed)return false;//一直以来都没按下
    if(!lastPressed){//没按下->按下
        startPress();
        lastPressed=true;
        lastTouchTime=millis();
        uiRender();
    }else if(nowPressed){//按下->一直按着
        touchTime=millis()-lastTouchTime;
        if(touchTime>clickTimeMax){
            if(!isLongPressing){
                isLongPressing=true;
                startLongPress();
            }
            else longPressing();
        }
        lowRender();
    }else{//按下->没按下
        lastPressed=false;
        if(isLongPressing){
            endLongPress();
            isLongPressing=false;
        }else{
            endClick();
        }
        uiRender();
    }
    return true;
}

TFT_eSPI tft = TFT_eSPI(); // 创建 TFT 对象
std::vector<uiActivity*> uiActivityPool;
uiActivity* controlActivityPtr = nullptr;
uiActivity* renderActivityPtr = nullptr;

uint32_t backgoundColor = TFT_WHITE;
void nullFunc(){
    //Need to be empty.
}

uiActivity* createActivity(char const* activityName){//创建一个activity,成功则返回新activity的指针,失败则返回nullptr
    for(auto activity : uiActivityPool){
        if(strcmp(activity->activityName,activityName)==0){
            return nullptr;
        }
    }
    uiActivity* newActivity = new uiActivity(activityName);
    uiActivityPool.push_back(newActivity);
    return newActivity;
}

uiActivity* getActivity(char const* activityName){//获取指定名字的activity
    for(auto activity : uiActivityPool){
        if(strcmp(activity->activityName,activityName)==0){
            return activity;
        }
    }
    return nullptr;
}
bool deleteActivity(char const* activityName){//删除一个activity,只返回是否成功
    for(auto it = uiActivityPool.begin(); it != uiActivityPool.end(); ++it){
        if(strcmp((*it)->activityName, activityName) == 0){
            delete *it;                           // 先删除对象
            uiActivityPool.erase(it);             // ✅ 正确！传递迭代器给erase
            return true;
        }
    }
    return false;
}
void uiRender(){
    if(renderActivityPtr == nullptr){
        return;
    }
    tft.fillScreen(backgoundColor);
    //Serial.println("Drawing element...");
    for(auto element : renderActivityPtr->uiElementsPool){
        element->draw(); // 调用每个元素的 draw 方法进行绘制
    }
}
void lowRender(){
    static uint16_t renderTimer = 0;
    if(renderTimer<1-0){
        renderTimer++;
        return;
    }
    renderTimer=0;
    
    uiRender();
}
void btnMgr(){
    for(int i = renderActivityPtr->uiButtonsPool.size() - 1; i >= 0; i--){
        if(renderActivityPtr->uiButtonsPool[i]->touchResponse()){
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
void popUp(char const* text){
    #define sideLength 10//定义popUp方框的边长
    backBtn = new uiButton(text,sideLength,sideLength,popUpInverseColor,320-2*sideLength,240-2*sideLength,0xFFFF);
    backBtn->startPressCallback = popUpInverseColor;//神秘屎山，这么写只是为了提供一个按钮遮罩，让弹窗界面覆盖在所有按钮之前，阻止下方按钮被点击，并且弹窗界面被按下不变色
    backBtn->endLongPressCallback = popUpInverseColor;
    closeBtn = new uiButton("X",320-35-sideLength,sideLength,closePopup,35,35,TFT_LIGHTGREY);
}

bool enteredNum = false;
//一下代码原本是为了用键盘输入数字而准备，但是因为工程量较大所以放弃，准备改用已有的slider输入数字
/* double uiInputNumber(){ 
    enteredNum = false;
    uiActivity* tempControlPtr = controlActivityPtr;
    uiActivity* tempRenderPtr = renderActivityPtr;
    controlActivityPtr = createActivity("InputNumber");
    renderActivityPtr = controlActivityPtr;
    for(int i=0;i<10;i++){
        uiInputNumArray[i]=0;
    }
    uiText* uiNumArray[10];
    for(int i=0;i<10;i++){ 
        uiNumArray[i] = new uiText("0",i*25+10,10,2,TFT_BLACK);
    }




    controlActivityPtr = tempControlPtr;
    renderActivityPtr = tempRenderPtr;
} */
double uiInputNumberSliderX100(){
    enteredNum = false;
    uiActivity* tempControlPtr = controlActivityPtr;
    uiActivity* tempRenderPtr = renderActivityPtr;
    controlActivityPtr = createActivity("InputNumber");
    renderActivityPtr = controlActivityPtr;

    uiSlider* slider = new uiSlider("Control", 25, 120, 270, 0.0);
    new uiText("Your input will x100", 10, 50, 2, TFT_BLACK);
    new uiButton("OK", 135, 150, [](){
        enteredNum = true;
    }, 50, 25, TFT_CYAN, TFT_BLACK);
    uiRender();
    while(!enteredNum){
        getTouch();
        btnMgr();
        vTaskDelay(5);
    }
    double result = slider->percentage*100;
    controlActivityPtr = tempControlPtr;
    renderActivityPtr = tempRenderPtr;
    deleteActivity("InputNumber");
    return result;
}

