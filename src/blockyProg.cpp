#include "blockyProg.h"
#include "Arduino.h"

uint8_t targetMac[6] = {0xDC,0xB4,0xD9,0x21,0x59,0x88};

static const ioDefine kSelectableIos[4] = {IO1, IO2, IO19, IO20};
static uiSlider* gIoSelectSlider = nullptr;
static int gDigitalSelection = -1;
static int gDeleteConfirmResult = -1;
static int gDeleteBlockIndex = -1;

static bool confirmDeleteBlock();
static void tryDeleteBlock(uint8_t index);
static void requestDeleteBlockByIndex(uint8_t index);

class blockyDeleteButton : public uiButton{
    public:
    uint8_t blockIndex;
    blockyDeleteButton(const char* text, int16_t x, int16_t y, uint8_t blockIndex, int16_t width=50, int16_t height=25, uint32_t backgroundColor=TFT_CYAN, uint32_t textColor=TFT_BLACK)
        : uiButton(text, x, y, nullFunc, width, height, backgroundColor, textColor) {
        this->blockIndex = blockIndex;
    }
    protected:
    void endClick(){
        inverseColor();
        if(lastInTheBtn){
            requestDeleteBlockByIndex(blockIndex);
        }
    }
};

static void setEnteredNum(){
    enteredNum = true;
}

static void selectDigitalHigh(){
    gDigitalSelection = 1;
    enteredNum = true;
}

static void selectDigitalLow(){
    gDigitalSelection = 0;
    enteredNum = true;
}

static void confirmDeleteYes(){//确认按钮回调
    gDeleteConfirmResult = 1;
}

static void confirmDeleteNo(){//取消按钮回调
    gDeleteConfirmResult = 0;
}

static bool confirmDeleteBlock(){//询问是否要删除
    gDeleteConfirmResult = -1;
    uiActivity* tempControlPtr = controlActivityPtr;
    uiActivity* tempRenderPtr = renderActivityPtr;
    controlActivityPtr = createActivity("DeleteBlockConfirm");
    renderActivityPtr = controlActivityPtr;

    new uiText("Delete this block?", 65, 70, 2, TFT_BLACK);
    new uiButton("Yes", 70, 130, confirmDeleteYes, 70, 35, TFT_RED, TFT_WHITE);
    new uiButton("No", 180, 130, confirmDeleteNo, 70, 35, TFT_CYAN, TFT_BLACK);

    uiRender();
    while(gDeleteConfirmResult < 0){
        getTouch();
        btnMgr();
        vTaskDelay(5);
    }

    bool result = (gDeleteConfirmResult == 1);
    controlActivityPtr = tempControlPtr;
    renderActivityPtr = tempRenderPtr;
    deleteActivity("DeleteBlockConfirm");
    return result;
}

static void tryDeleteBlock(uint8_t index){//正式开始删除
    if(index == 0 || index >= blockyPool.size()){
        return;
    }
    blockyBase* toDelete = blockyPool[index];
    blockyPool.erase(blockyPool.begin() + index);
    delete toDelete;
    for(uint8_t i = 0; i < blockyPool.size(); i++){
        blockyPool[i]->index = i;
        blockyPool[i]->updateFrame();
    }
}

static void requestDeleteBlockByIndex(uint8_t index){//由被点击按钮触发删除确认
    gDeleteBlockIndex = index;
    if(gDeleteBlockIndex == 0 || gDeleteBlockIndex >= blockyPool.size()){
        return;
    }
    if(confirmDeleteBlock()){
        tryDeleteBlock(gDeleteBlockIndex);
    }
    gDeleteBlockIndex = -1;
    renderActivityPtr = getActivity("blockyProgMain");
    uiRender();
}

static int ioToIndex(ioDefine io){//将io转换为0-3的索引，方便slider使用
    for(int i = 0; i < 4; i++){
        if(kSelectableIos[i] == io){
            return i;
        }
    }
    return 0;
}

static ioDefine indexToIo(int idx){//将0-3的索引转换为io
    if(idx < 0) idx = 0;
    if(idx > 3) idx = 3;
    return kSelectableIos[idx];
}

static const char* ioToText(ioDefine io){//将io转换为文本，方便显示
    switch(io){
        case IO1: return "IO1";
        case IO2: return "IO2";
        case IO19: return "IO19";
        case IO20: return "IO20";
        default: return "IO?";
    }
}

static void drawSelectedIoPreview(){//在选择IO的界面上绘制当前选择的IO
    if(gIoSelectSlider == nullptr){
        return;
    }
    int idx = (int)(gIoSelectSlider->percentage * 3.0f + 0.5f);
    ioDefine selected = indexToIo(idx);
    static char ioBuffer[20];
    snprintf(ioBuffer, sizeof(ioBuffer), "%s", ioToText(selected));
    tft.drawString(ioBuffer, 120, 90, 2);
}

static ioDefine selectIoWithSlider(ioDefine defaultIo = IO1){//通过slider选择IO的界面，返回选择的IO
    enteredNum = false;
    uiActivity* tempControlPtr = controlActivityPtr;
    uiActivity* tempRenderPtr = renderActivityPtr;
    controlActivityPtr = createActivity("SelectBlockyIO");
    renderActivityPtr = controlActivityPtr;

    float initPer = ioToIndex(defaultIo) / 3.0f;
    gIoSelectSlider = new uiSlider("Control", 25, 120, 270, initPer);
    new uiText("Select IO: 1/2/19/20", 10, 50, 2, TFT_BLACK);
    new uiButton("OK", 135, 150, setEnteredNum, 50, 25, TFT_CYAN, TFT_BLACK);
    new uiDrawCallback(drawSelectedIoPreview);

    uiRender();
    while(!enteredNum){
        getTouch();
        btnMgr();
        vTaskDelay(5);
    }

    int idx = (int)(gIoSelectSlider->percentage * 3.0f + 0.5f);//巧妙地选择IO
    ioDefine result = indexToIo(idx);
    gIoSelectSlider = nullptr;
    controlActivityPtr = tempControlPtr;
    renderActivityPtr = tempRenderPtr;
    deleteActivity("SelectBlockyIO");
    return result;
}

static int selectDigitalValue(){
    enteredNum = false;
    gDigitalSelection = -1;
    uiActivity* tempControlPtr = controlActivityPtr;
    uiActivity* tempRenderPtr = renderActivityPtr;
    controlActivityPtr = createActivity("SelectDigitalValue");
    renderActivityPtr = controlActivityPtr;

    new uiText("Digital Output", 90, 60, 2, TFT_BLACK);
    new uiButton("HIGH", 50, 120, selectDigitalHigh, 90, 35, TFT_CYAN, TFT_BLACK);
    new uiButton("LOW", 180, 120, selectDigitalLow, 90, 35, TFT_CYAN, TFT_BLACK);

    uiRender();
    while(gDigitalSelection < 0){
        getTouch();
        btnMgr();
        vTaskDelay(5);
    }

    controlActivityPtr = tempControlPtr;
    renderActivityPtr = tempRenderPtr;
    deleteActivity("SelectDigitalValue");
    return gDigitalSelection;
}

static void addDigitalOutputBlock(){
    ioDefine io = selectIoWithSlider(IO1);//阻塞式进程，直到用户选择IO
    int value = selectDigitalValue();//阻塞式进程，直到用户选择HIGH还是LOW
    blockyPool.push_back(new blockyDigitalWrite(blockyPool.size(), io, value));
    switchToBlockyProgMain();
    uiRender();
}

static void addAnalogOutputBlock(){
    ioDefine io = selectIoWithSlider(IO1);//同理
    int dutyPercent = (int)(uiInputNumberSliderX100() + 0.5f);
    if(dutyPercent < 0) dutyPercent = 0;
    if(dutyPercent > 100) dutyPercent = 100;
    blockyPool.push_back(new blockyAnalogWrite(blockyPool.size(), io, dutyPercent));
    switchToBlockyProgMain();
    uiRender();
}

std::vector<blockyBase*> blockyPool;
uiSlider* blockyControlSlider;
int16_t blockyStartX = 10;
#define blockyMoveMode 1 //0:移动的方向是砖块移动的方向 1:移动的方向是视野移动的方向
void blockyStart::updateFrame(){
    x = blockyStartX + getBlockyLength(index);
    btn1->x = x;
}
void blockyStart::execute(){
    // Start block本身不执行任何操作，所有逻辑都在runBlockyProgram里实现
}
blockyStart::~blockyStart(){
    delete btn1;
}
blockyStart::blockyStart(uint8_t index){
    this->index = index;
    width = 50;
    x = blockyStartX + getBlockyLength(index);
    btn1 = new uiButton("Start", x, 10, runBlockyProgram, width, 100);
}

void blockyDigitalWrite::updateFrame(){
    x = blockyStartX + getBlockyLength(index);
    btn1->x = x;
    static_cast<blockyDeleteButton*>(btn1)->blockIndex = index;
}
void blockyDigitalWrite::execute(){
    BCPpinMode(targetMac, io, digitalWriteMode);
    BCPdigitalWrite(targetMac, io, value);
}
blockyDigitalWrite::blockyDigitalWrite(uint8_t index, ioDefine io, int8_t value){
    this->index = index;
    this->io = io;
    this->value = value;
    width = 70;
    x = blockyStartX + getBlockyLength(index);
    snprintf(label, sizeof(label), "D%s=%s", ioToText(io), value ? "H" : "L");
    btn1 = new blockyDeleteButton(label, x, 10, this->index, width, 100, TFT_GREEN, TFT_BLACK);
}
blockyDigitalWrite::~blockyDigitalWrite(){
    delete btn1;
}

void blockyAnalogWrite::updateFrame(){
    x = blockyStartX + getBlockyLength(index);
    btn1->x = x;
    static_cast<blockyDeleteButton*>(btn1)->blockIndex = index;
}
void blockyAnalogWrite::execute(){
    BCPpinMode(targetMac, io, analogWriteMode);
    int pwm = (dutyPercent * 255) / 100;
    BCPanalogWrite(targetMac, io, pwm);
}
blockyAnalogWrite::blockyAnalogWrite(uint8_t index, ioDefine io, int dutyPercent){
    this->index = index;
    this->io = io;
    this->dutyPercent = dutyPercent;
    width = 90;
    x = blockyStartX + getBlockyLength(index);
    snprintf(label, sizeof(label), "A%s=%d%%", ioToText(io), dutyPercent);
    btn1 = new blockyDeleteButton(label, x, 10, this->index, width, 100, TFT_ORANGE, TFT_BLACK);
}
blockyAnalogWrite::~blockyAnalogWrite(){
    delete btn1;
}

void runBlockyProgram(){
    for(size_t i = 1; i < blockyPool.size(); i++){
        blockyPool[i]->execute();
        delay(1000);
    }
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
    initBCP();
    pairDevice(targetMac);
    initIoMsg();

    uiActivity* tempControlPtr = controlActivityPtr;
    //blocky编程的主activity
    controlActivityPtr = createActivity("blockyProgMain");
    blockyControlSlider = new uiSlider("Control", 25, 220, 270, 0.0);
    new uiButton("<",10,170,blockyMoveLeft);
    new uiButton(">",260,170,blockyMoveRight);
    new uiButton("Add", 70, 170, switchToBlockyProgAdd);
//这是一种输入数字实例，可供参考
/*     new uiButton("Input", 130, 170, [](){
        double num = uiInputNumberSliderX100();
        static char buffer[48];
        snprintf(buffer, sizeof(buffer), "You inputted: %.2f", num);
        popUp(buffer);
    }); */
    new uiDrawCallback(blockyMgr);
    blockyPool.push_back(new blockyStart(blockyPool.size()));
    //uiButton *btn = new uiButton("Start", 10, 10, nullFunc, 50, 25);

    //blocky编程的添加block的activity
    controlActivityPtr = createActivity("blockyProgAdd");
    new uiText("1/1",10,190,2);
    new uiButton("<",10,205);
    new uiButton(">",260,205);
    new uiButton("Digital", 20, 60, addDigitalOutputBlock, 120, 50, TFT_GREEN, TFT_BLACK);
    new uiButton("Analog", 180, 60, addAnalogOutputBlock, 120, 50, TFT_ORANGE, TFT_BLACK);
    new uiButton("Back", 70, 205, switchToBlockyProgMain);
    tft.drawString("Add Block", 70, 190, TFT_BLACK);
    controlActivityPtr = tempControlPtr;
}
void switchToBlockyProgAdd(){
    renderActivityPtr = getActivity("blockyProgAdd");
    uiRender();
}
void switchToBlockyProgMain(){
    renderActivityPtr = getActivity("blockyProgMain");
    uiRender();
}


int16_t getBlockyLength(uint8_t index){
    int16_t length=0;
    for(uint8_t i=0; i<index; i++){
        length+=blockyPool[i]->width;
    }
    return length;
}