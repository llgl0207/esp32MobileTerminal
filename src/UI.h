#include <vector>
#include "TFT_eSPI.h"
#include "Touch.h"

#ifndef UI_H
#define UI_H
    class uiElementBase;
    class uiButtonBase;
    class uiButton;
    class uiText;
    extern TFT_eSPI tft;
    extern std::vector<uiElementBase*> uiElementsPool; // 存储所有 UI 元素的容器
    extern std::vector<uiButtonBase*> uiButtonsPool; // 存储所有按钮的容器

    class uiElementBase{
        public:
            virtual void draw() = 0; // 纯虚函数，强制子类实现
            uiElementBase(){
                uiElementsPool.push_back(this); // 将当前对象添加到容器中
            }
            ~uiElementBase(){
                // 从容器中移除当前对象
                auto it = find(uiElementsPool.begin(), uiElementsPool.end(), this);
                uiElementsPool.erase(it);
            }
    };

    class uiButtonBase : public uiElementBase{
        protected:
            long touchTime=0;
            long lastTouchTime=0;
            long clickTimeMax=200;//指定时间内的触摸被认为是一次点击
            bool isPressed=false;
            bool isLongPressing=false;
            bool nowPressed(){
                if(touchX>=x&&touchX<=x+width&&touchY>=y&&touchY<=y+height)
                    return true;
                else
                    return false;
            }
        public:
            // 定义触摸响应函数，根据触摸状态调用不同的事件处理函数
            // 虚函数请在子类中实现
            virtual void startPress(){};
            virtual void endClick(){};
            virtual void startLongPress(){};
            virtual void longPressing(){};
            virtual void endLongPress(){};
            bool touchResponse(){
                if(!isPressed&&!nowPressed())return false;//一直以来都没按下
                else if(!isPressed){//没按下->按下
                    startPress();
                    isPressed=true;
                    lastTouchTime=millis();
                }else if(nowPressed()){//按下->一直按着
                    touchTime=millis()-lastTouchTime;
                    if(touchTime>clickTimeMax){
                        if(!isLongPressing)startLongPress();
                        else longPressing();
                    }
                }else{//按下->没按下
                    isPressed=false;
                    if(isLongPressing){
                        endLongPress();
                        isLongPressing=false;
                    }else{
                        endClick();
                    }
                }
                return true;
            }
            uiButtonBase(){
                uiButtonsPool.push_back(this); // 将当前对象添加到容器中
            }
            ~uiButtonBase(){
                // 从容器中移除当前对象
                auto it = find(uiButtonsPool.begin(), uiButtonsPool.end(), this);
                uiButtonsPool.erase(it);
            }
        protected:
            uint16_t x, y, width, height; // 按钮的位置和尺寸
            const char* text; // 按钮的文本
            uint32_t backgroundColor; // 按钮的背景颜色
            uint32_t textColor; // 按钮的文本颜色


    };

    class uiButton : public uiButtonBase{
        protected:
            void startPress(){
                backgroundColor = ~backgroundColor; // 按下时切换背景颜色
                textColor = ~textColor;
            }
            void endClick(){
                backgroundColor = ~backgroundColor; // 抬起时切换背景颜色
                textColor = ~textColor;
            }
            void endLongPress(){
                backgroundColor = ~backgroundColor; // 抬起时切换背景颜色
                textColor = ~textColor;
            }
        public:

            uiButton(const char* text, uint16_t x, uint16_t y, uint16_t width=50, uint16_t height=25, uint32_t backgroundColor=TFT_CYAN, uint32_t textColor=TFT_BLACK){
                this->x = x;
                this->y = y;
                this->width = width;
                this->height = height;
                this->text = text;
                this->backgroundColor = backgroundColor;
                this->textColor = textColor;
            }
            void draw(){
                // 绘制按钮
                tft.fillRect(x, y, width, height, backgroundColor);
                uint32_t lastBackgroundColor = tft.textbgcolor;
                uint32_t lastColor = tft.textcolor;
                tft.setTextColor(textColor,backgroundColor);      // 设置文本颜色
                tft.drawString(text,             // 文本内容
                              x + (width - tft.textWidth(text)) / 2,  // X坐标
                              y + (height - tft.fontHeight()) / 2);   // Y坐标
                tft.setTextColor(lastColor, lastBackgroundColor); // 恢复之前的颜色设置
            }

    };

    class uiText : public uiElementBase{
        public:
            const char* Str;
            uint16_t x, y; // 文本的位置
            uint32_t color; // 文本的颜色
            void draw(){
                uint32_t lastBackgroundColor = tft.textbgcolor;
                uint32_t lastColor = tft.textcolor;
                tft.setTextColor(color, lastBackgroundColor);      // 设置文本颜色和背景颜色
                tft.drawString(Str, x, y);    // 绘制文本
                tft.setTextColor(lastColor, lastBackgroundColor); // 恢复之前的颜色设置
            }
    };

    void uiRender();
    void btnMgr();
#endif