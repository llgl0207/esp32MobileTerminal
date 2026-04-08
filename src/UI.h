#include <vector>
#include<Arduino.h>
#include "TFT_eSPI.h"
#include "Touch.h"

#ifndef UI_H
#define UI_H
    extern uint32_t backgoundColor; // 全局背景色（RGB565）
    extern bool enteredNum;
    // 立即执行一帧完整渲染。
    // 输入: 无。输出: 无。
    // 功能: 清屏并绘制当前renderActivityPtr下的全部UI元素。
    void uiRender();

    // 降频渲染函数。
    // 输入: 无。输出: 无。
    // 功能: 通过内部计数器降低重绘频率，减少长按拖拽时的刷新负载。
    void lowRender();

    // 触摸事件分发器。
    // 输入: 无。输出: 无。
    // 功能: 倒序遍历按钮池，保证后绘制的控件优先响应触摸。
    void btnMgr();

    // 弹窗遮罩反色回调。
    // 输入/输出: 无。功能: 通过反色反馈遮罩被按下状态。
    void popUpInverseColor();

    // 创建简易弹窗。
    // 输入: text 为弹窗显示文本。
    // 输出: 无。功能: 创建全屏遮罩按钮和关闭按钮。
    void popUp(char const* text);

    // 空操作回调。
    // 输入/输出: 无。功能: 用作默认函数指针，避免空指针调用。
    void nullFunc();
    //extern uint8_t uiInputNumArray[10];//这两行原本是为了键盘输入数字，但是工程量来不及废弃了
    //double uiInputNumber();
    // 通过滑块输入0~100范围的数值。
    // 输入: 无。
    // 输出: double，物理意义为百分比量（0~100），可用于占空比等参数输入。
    double uiInputNumberSliderX100();
    class uiElementBase;
    class uiButtonBase;
    class uiButton;
    class uiText;
    extern TFT_eSPI tft;
    class uiActivity{
        public:
        char const* activityName;//活动名称
        std::vector<uiElementBase*> uiElementsPool; // 存储所有 UI 元素的容器
        std::vector<uiButtonBase*> uiButtonsPool; // 存储所有按钮的容器
        uiActivity(char const* activityName){
            this->activityName = activityName;
        }
        ~uiActivity();

    };
    extern std::vector<uiActivity*> uiActivityPool;
    extern uiActivity* controlActivityPtr;
    extern uiActivity* renderActivityPtr;

    // 创建活动页面。
    // 输入: activityName 为页面名称。
    // 输出: 成功返回新页面指针；同名已存在返回nullptr。
    uiActivity* createActivity(char const* activityName);

    // 按名称获取活动页面。
    // 输入: activityName 为页面名称。
    // 输出: 页面指针；未找到返回nullptr。
    uiActivity* getActivity(char const* activityName);

    // 删除活动页面并释放其控件。
    // 输入: activityName 为页面名称。
    // 输出: true 删除成功，false 表示未找到。
    bool        deleteActivity(char const* activityName);
    

    class uiElementBase{
        public:
            uiActivity* thisActivity;
            virtual void draw() = 0; // 纯虚函数，强制子类实现
            uiElementBase(){
                thisActivity = controlActivityPtr;
                thisActivity->uiElementsPool.push_back(this); // 将当前对象添加到容器中
            }
            ~uiElementBase();
    };

    class uiDrawCallback: public uiElementBase{
        public:
        void (*drawCallback)()=nullFunc;
        uiDrawCallback(void (*callbackPtr)()){
            this->drawCallback = callbackPtr;
        }
        void draw(){
            drawCallback();
        }

    };

    class uiButtonBase : public uiElementBase{
        protected:
            long touchTime=0;
            long lastTouchTime=0;
            long clickTimeMax=200;//指定时间内的触摸被认为是一次点击
            bool lastPressed=false;
            bool nowPressed=false;
            bool lastInTheBtn=false;
            bool nowInTheBtn=false;
            bool isLongPressing=false;
            void Pressed();
        public:
            void (*startPressCallback)()=nullFunc;
            void (*endClickCallback)()=nullFunc;
            void (*startLongPressCallback)()=nullFunc;
            void (*longPressingCallback)()=nullFunc;
            void (*endLongPressCallback)()=nullFunc;
            // 定义触摸响应函数，根据触摸状态调用不同的事件处理函数
            // 虚函数请在子类中实现
            virtual void startPress(){startPressCallback();};
            virtual void endClick(){endClickCallback();};
            virtual void startLongPress(){startLongPressCallback();};
            virtual void longPressing(){longPressingCallback();};
            virtual void endLongPress(){endLongPressCallback();};

            
            bool touchResponse();
            uiButtonBase(){
                thisActivity->uiButtonsPool.push_back(this); // 将当前对象添加到容器中
            }
            ~uiButtonBase(){
                // 从容器中移除当前对象
                auto it = find(thisActivity->uiButtonsPool.begin(), thisActivity->uiButtonsPool.end(), this);
                if(it != thisActivity->uiButtonsPool.end()){
                    thisActivity->uiButtonsPool.erase(it);
                }
            }
        public:
            int16_t x, y, width, height; // 按钮的位置和尺寸
            const char* text; // 按钮的文本
            uint32_t backgroundColor; // 按钮的背景颜色
            uint32_t textColor; // 按钮的文本颜色


    };

       class uiButton : public uiButtonBase{
        public:
            void inverseColor(){
                backgroundColor = ~backgroundColor; // 按下时切换背景颜色
                textColor = ~textColor;
            }
            uiButton(const char* text, int16_t x, int16_t y,void (*clickFuncPtr)() = nullFunc, int16_t width=50, int16_t height=25, uint32_t backgroundColor=TFT_CYAN, uint32_t textColor=TFT_BLACK){
                this->x = x;
                this->y = y;
                this->endClickCallback = clickFuncPtr;
                this->width = width;
                this->height = height;
                this->text = text;
                this->backgroundColor = backgroundColor;
                this->textColor = textColor;
            }
            void draw(){
                // 绘制按钮
                tft.fillRect(x, y, width, height, backgroundColor);
                tft.drawRect(x, y, width, height, TFT_BLACK);//绘制边框
                uint32_t lastBackgroundColor = tft.textbgcolor;
                uint32_t lastColor = tft.textcolor;
                tft.setTextColor(textColor,backgroundColor);      // 设置文本颜色
                tft.drawString(text,             // 文本内容
                              x + (width - tft.textWidth(text)) / 2,  // X坐标
                              y + (height - tft.fontHeight()) / 2);   // Y坐标
                tft.setTextColor(lastColor, lastBackgroundColor); // 恢复之前的颜色设置

            }
        protected:
            void startPress(){
                inverseColor();
                startPressCallback();
            }
            void endClick(){
                inverseColor();
                if(lastInTheBtn){//只有在判定为短按且在按钮内的时候才触发点击事件
                    endClickCallback();
                }
            }
            void longPressing(){
                longPressingCallback();
            }
            void endLongPress(){
                inverseColor();
                endLongPressCallback();
            }

    };

    class uiDragButton : public uiButton{
        protected:
            void longPressing(){
                x+=deltaTouchX;
                y+=deltaTouchY;
            }
        public:
            uiDragButton(const char* text, int16_t x, int16_t y, int16_t width=50, int16_t height=25, uint32_t backgroundColor=TFT_CYAN, uint32_t textColor=TFT_BLACK)
                : uiButton(text, x, y, nullFunc, width, height, backgroundColor, textColor) {
                    clickTimeMax=0;
            }
    };

        class uiSlider : public uiButton{
        public:
            float percentage;
        protected:
            int16_t slideX;
            int16_t slideY;
            int16_t slideRange;
            
            char textBuffer[20];
            void longPressing(){
                x+=deltaTouchX;//处理位移
                if(x<slideX-width/2)x=slideX-width/2;//防止滑块滑出
                if(x>slideX+slideRange-width/2)x=slideX+slideRange-width/2;//防止滑块滑出
                percentage=(x+width/2-slideX)/(float)slideRange;//计算比例
                longPressingCallback();
            }
        public:
            uiSlider(const char* text, int16_t x, int16_t y, int16_t slideRange, float percentage, int16_t width=50, int16_t height=25, uint32_t backgroundColor=TFT_CYAN, uint32_t textColor=TFT_BLACK)
                : uiButton(text, x+slideRange*percentage-width/2, y-height/2, nullFunc, width, height, backgroundColor, textColor) {
                this->slideRange=slideRange;
                this->percentage=percentage;
                slideX=x;
                slideY=y;
                clickTimeMax=0;
            }
            void draw(){
                tft.drawLine(slideX, slideY, slideX+slideRange, slideY, TFT_RED);
                sprintf(textBuffer,"%.2f",percentage);
                text=textBuffer;
                x=slideX+slideRange*percentage-width/2;
                //tft.drawFloat(percentage,2,0,0);
                uiButton::draw();
            }

            /* void writePer(float newPer){
                percentage=newPer;
                x=slideX+slideRange*percentage-width/2;
            } */
    };

    class uiText : public uiElementBase{
        public:
            const char* Str;
            int16_t x, y; // 文本的位置
            uint32_t color; // 文本的颜色
            uint8_t font;
            uiText(const char* Str, int16_t x, int16_t y, uint8_t font=0, uint32_t color=TFT_BLACK)
                : Str(Str), x(x), y(y), font(font), color(color) {
            }
            void draw(){
                uint32_t lastBackgroundColor = tft.textbgcolor;
                uint32_t lastColor = tft.textcolor;
                tft.setTextColor(color, lastBackgroundColor);      // 设置文本颜色和背景颜色
                tft.drawString(Str, x, y, font);    // 绘制文本
                tft.setTextColor(lastColor, lastBackgroundColor); // 恢复之前的颜色设置
            }
    };
#endif