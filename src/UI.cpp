#include "UI.h"

// 函数功能: 释放Activity内全部UI元素。
// 输入/输出: 无。
// 算法: 反向删除，避免vector迭代器在元素析构时失效。
uiActivity::~uiActivity(){
    // Elements remove themselves from uiElementsPool in their destructors.
    // Delete from back until empty to avoid iterator invalidation.
    while(!uiElementsPool.empty()){
        delete uiElementsPool.back();
    }
}

// 函数功能: UI基础元素析构，从所属Activity元素池中摘除自己。
// 输入/输出: 无。
uiElementBase::~uiElementBase(){
    // 从容器中移除当前对象
    auto it = find(thisActivity->uiElementsPool.begin(), thisActivity->uiElementsPool.end(), this);
    if(it != thisActivity->uiElementsPool.end()){
        thisActivity->uiElementsPool.erase(it);
    }
}

// 函数功能: 更新按钮当前帧按压状态。
// 输入参数: 依赖全局触摸坐标touchX/touchY。
// 输出参数: 无，更新nowPressed/nowInTheBtn等内部状态。
// 算法说明: 采用“按下捕获”策略，按钮按住后即便手指滑出也保持按压态。
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

// 函数功能: 执行按钮状态机并触发事件回调。
// 输入: 无（读取内部按压状态与触摸时间）。
// 输出: bool，true表示当前按钮消费了本次触摸事件。
// 算法说明: 三态转换
// 1) 未按->按下: startPress
// 2) 按下保持: 短按窗口内等待，超时进入longPressing
// 3) 按下->释放: 根据是否长按触发endClick或endLongPress
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

// 函数功能: 空回调占位符。
// 输入/输出: 无。
void nullFunc(){
    //Need to be empty.
}

// 函数功能: 创建一个UI活动页。
// 输入参数: activityName -> 页面名称。
// 输出参数: 新页面指针；若重名则返回nullptr。
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

// 函数功能: 按名称查找活动页。
// 输入参数: activityName -> 页面名称。
// 输出参数: 活动页指针；未找到返回nullptr。
uiActivity* getActivity(char const* activityName){//获取指定名字的activity
    for(auto activity : uiActivityPool){
        if(strcmp(activity->activityName,activityName)==0){
            return activity;
        }
    }
    return nullptr;
}

// 函数功能: 删除指定活动页并释放对象。
// 输入参数: activityName -> 页面名称。
// 输出参数: bool，表示删除是否成功。
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

// 函数功能: 渲染当前页面的完整一帧。
// 输入/输出: 无。
// 语句组功能: 先清屏，再按注册顺序绘制元素池。
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

// 函数功能: 低频渲染，减少频繁重绘导致的闪烁和性能开销。
// 输入/输出: 无。
void lowRender(){
    static uint16_t renderTimer = 0;
    if(renderTimer<1-0){
        renderTimer++;
        return;
    }
    renderTimer=0;
    
    uiRender();
}

// 函数功能: 按优先级分发触摸事件。
// 输入/输出: 无。
// 算法: 倒序扫描按钮池，命中后立刻停止，模拟Z轴层级点击。
void btnMgr(){
    for(int i = renderActivityPtr->uiButtonsPool.size() - 1; i >= 0; i--){
        if(renderActivityPtr->uiButtonsPool[i]->touchResponse()){
            break; // 如果有按钮响应了触摸事件，停止检查其他按钮
        }
    }
}
uiButton* backBtn;
uiButton* closeBtn;

// 函数功能: 关闭弹窗并释放遮罩/关闭按钮。
// 输入/输出: 无。
void closePopup(){
    delete backBtn;
    delete closeBtn;
}

// 函数功能: 弹窗遮罩按下时的视觉反馈。
// 输入/输出: 无。
void popUpInverseColor(){
    backBtn->inverseColor();
}

// 函数功能: 构建简单弹窗层。
// 输入参数: text -> 显示文本。
// 输出参数: 无。
// 语句组功能: 创建全屏遮罩按钮拦截下层点击，再创建右上角关闭按钮。
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
// 函数功能: 通过滑块阻塞式输入一个百分比数值。
// 输入参数: 无。
// 输出参数: double，范围约0~100，物理意义可映射为占空比/比例系数。
// 算法说明: 临时切换到输入Activity，循环读取触摸直到OK按钮置位enteredNum。
double uiInputNumberSliderX100(){
    enteredNum = false;
    uiActivity* tempControlPtr = controlActivityPtr;
    uiActivity* tempRenderPtr = renderActivityPtr;
    controlActivityPtr = createActivity("InputNumber");
    renderActivityPtr = controlActivityPtr;

    // 语句组功能: 创建输入界面控件（滑条+提示+确认按钮）。
    uiSlider* slider = new uiSlider("Control", 25, 120, 270, 0.0);
    new uiText("Your input will x100", 10, 50, 2, TFT_BLACK);
    new uiButton("OK", 135, 150, [](){
        enteredNum = true;
    }, 50, 25, TFT_CYAN, TFT_BLACK);
    uiRender();
    // 语句组功能: 本地事件循环，直到用户确认输入。
    while(!enteredNum){
        getTouch();
        btnMgr();
        vTaskDelay(5);
    }
    // 语句组功能: 恢复原页面并返回结果。
    double result = slider->percentage*100;
    controlActivityPtr = tempControlPtr;
    renderActivityPtr = tempRenderPtr;
    deleteActivity("InputNumber");
    return result;
}

