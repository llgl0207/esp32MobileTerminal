#include "blockyProg.h"
std::vector<blockyBase*> blockyPool;
uiSlider* blockyControlSlider;
int16_t blockyStartX = 10;
#define blockyMoveMode 1 //0:移动的方向是砖块移动的方向 1:移动的方向是视野移动的方向
void blockyStart::updateFrame(){
    x = blockyStartX + getBlockyLength(index);
    btn1->x = x;
}
blockyStart::blockyStart(uint8_t index = blockyPool.size()){
    this->index = index;
    width = 50;
    x = blockyStartX + getBlockyLength(index);
    btn1 = new uiButton("Start", x, 10, nullFunc, width, 100);
}
void blockyMgr(){
    if(getBlockyLength()>320){
        blockyStartX = -(getBlockyLength()-320)*blockyControlSlider->percentage;
    }else{
        blockyStartX = 10;
    }
    for(uint8_t i=0; i<blockyPool.size(); i++){
        blockyPool[i]->updateFrame();
    }
}

void blockyMoveLeft(){
    if(getBlockyLength()<=320)return;//如果砖块长度不超过屏幕宽度则不需要移动
    #if blockyMoveMode == 0
        int dx = -20;
        if(blockyStartX+dx+getBlockyLength()>320){//只在右边没到顶的时候才能砖块左移
            blockyStartX += dx;
        }else{
            blockyStartX = -(getBlockyLength()-320);
        }
    #elif blockyMoveMode == 1
        int dx = 20;
        if(blockyStartX+dx<0){//只在左边没到顶的时候才能砖块右移
            blockyStartX += dx;
        }else{
            blockyStartX = 0;
        }
    #endif
    blockyControlSlider->percentage = (-blockyStartX)/(float)(getBlockyLength()-320);
    
}
void blockyMoveRight(){
    if(getBlockyLength()<=320)return;//如果砖块长度不超过屏幕宽度则不需要移动
    #if blockyMoveMode == 0
        int dx = 20;
        if(blockyStartX+dx<0){//只在左边没到顶的时候才能砖块右移
            blockyStartX += dx;
        }else{
            blockyStartX = 0;
        }
    #elif blockyMoveMode == 1
        int dx = -20;
        if(blockyStartX+dx+getBlockyLength()>320){//只在右边没到顶的时候才能砖块左移
            blockyStartX += dx;
        }else{
            blockyStartX = -(getBlockyLength()-320);
        }
    #endif
    blockyControlSlider->percentage = (-blockyStartX)/(float)(getBlockyLength()-320);
}
void blockyProgInit(){
    uiActivity* tempControlPtr = controlActivityPtr;

    controlActivityPtr = createActivity("blockyProgInstance");

    blockyControlSlider = new uiSlider("Control", 25, 220, 270, 0.0);
    uiButton* leftBtn = new uiButton("<",10,170,blockyMoveLeft);
    uiButton* rightBtn = new uiButton(">",260,170,blockyMoveRight);
    uiDrawCallback* back = new uiDrawCallback(blockyMgr);
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    blockyPool.push_back(new blockyStart());
    //uiButton *btn = new uiButton("Start", 10, 10, nullFunc, 50, 25);
    controlActivityPtr = tempControlPtr;
}


int16_t getBlockyLength(uint8_t index){
    int16_t length=0;
    for(uint8_t i=0; i<index; i++){
        length+=blockyPool[i]->width;
    }
    return length;
}