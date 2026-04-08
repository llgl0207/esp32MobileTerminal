#include "blockyProg.h"
#include "Arduino.h"

// 目标从机MAC地址，所有积木动作都会下发到该设备。
uint8_t targetMac[6] = {0xDC,0xB4,0xD9,0x21,0x59,0x88};

// 当前版本仅允许在这4个IO中选择，便于UI滑块离散映射。
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
    // 函数功能: 重写点击行为，触发“删除确认”流程。
    // 输入/输出: 无。
    void endClick(){
        inverseColor();
        if(lastInTheBtn){
            requestDeleteBlockByIndex(blockIndex);
        }
    }
};

// 函数功能: 通用“确认输入完成”回调。
static void setEnteredNum(){
    enteredNum = true;
}

// 函数功能: 选择数字高电平。
static void selectDigitalHigh(){
    gDigitalSelection = 1;
    enteredNum = true;
}

// 函数功能: 选择数字低电平。
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

// 函数功能: 弹出删除确认对话框。
// 输入: 无。
// 输出: true表示用户确认删除，false表示取消。
// 算法说明: 创建临时Activity并阻塞轮询触摸，等待Yes/No回调写入结果。
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
    // 语句组功能: 本地事件循环，直到用户做出确认选择。
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

// 函数功能: 执行积木删除并重建索引。
// 输入: index -> 要删除的积木索引。
// 输出: 无。
// 算法说明: 删除后线性重排index，并刷新每块的显示位置。
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

// 函数功能: 发起删除请求（带确认）。
// 输入: index -> 被点击积木索引。
// 输出: 无。
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

// 函数功能: 将IO枚举映射为离散索引。
// 输入: io -> 物理引脚枚举。
// 输出: 0~3索引，便于与slider百分比互转。
static int ioToIndex(ioDefine io){//将io转换为0-3的索引，方便slider使用
    for(int i = 0; i < 4; i++){
        if(kSelectableIos[i] == io){
            return i;
        }
    }
    return 0;
}

// 函数功能: 将离散索引反向映射为IO枚举。
// 输入: idx -> 可能越界的索引。
// 输出: 合法IO枚举（越界会钳位）。
static ioDefine indexToIo(int idx){//将0-3的索引转换为io
    if(idx < 0) idx = 0;
    if(idx > 3) idx = 3;
    return kSelectableIos[idx];
}

// 函数功能: 把IO枚举转成短标签文本。
// 输入: io -> 物理引脚枚举。
// 输出: const char*，用于按钮/预览显示。
static const char* ioToText(ioDefine io){//将io转换为文本，方便显示
    switch(io){
        case IO1: return "IO1";
        case IO2: return "IO2";
        case IO19: return "IO19";
        case IO20: return "IO20";
        default: return "IO?";
    }
}

// 函数功能: 在IO选择界面绘制当前选中预览。
// 输入/输出: 无。
// 算法: 通过slider百分比四舍五入到0~3离散档位。
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

// 函数功能: 使用滑块选择目标IO。
// 输入: defaultIo -> 初始默认引脚。
// 输出: 用户最终选择的IO枚举。
// 算法说明: 创建临时Activity，通过滑块连续值映射到4档离散IO。
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
    // 语句组功能: 阻塞读取触摸直到用户点击OK。
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

// 函数功能: 选择数字输出值（HIGH/LOW）。
// 输入: 无。
// 输出: 1表示HIGH，0表示LOW。
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

// 函数功能: 创建数字输出积木。
// 输入/输出: 无（内部通过两个弹窗流程获取IO与电平）。
static void addDigitalOutputBlock(){
    ioDefine io = selectIoWithSlider(IO1);//阻塞式进程，直到用户选择IO
    int value = selectDigitalValue();//阻塞式进程，直到用户选择HIGH还是LOW
    blockyPool.push_back(new blockyDigitalWrite(blockyPool.size(), io, value));
    switchToBlockyProgMain();
    uiRender();
}

// 函数功能: 创建模拟输出积木。
// 输入/输出: 无（内部获取IO与占空比）。
// 语句组功能: 占空比结果会被钳位到0~100，避免异常输入。
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

// 函数功能: 更新Start积木位置。
void blockyStart::updateFrame(){
    x = blockyStartX + getBlockyLength(index);
    btn1->x = x;
}

// 函数功能: Start积木执行入口（占位）。
// 说明: 真正调度在runBlockyProgram中处理。
void blockyStart::execute(){
    // Start block本身不执行任何操作，所有逻辑都在runBlockyProgram里实现
}

// 函数功能: 析构Start积木，释放按钮资源。
blockyStart::~blockyStart(){
    delete btn1;
}

// 函数功能: 构造Start积木。
// 输入: index -> 程序顺序位置。
blockyStart::blockyStart(uint8_t index){
    this->index = index;
    width = 50;
    x = blockyStartX + getBlockyLength(index);
    btn1 = new uiButton("Start", x, 10, runBlockyProgram, width, 100);
}

// 函数功能: 更新数字积木位置并同步删除按钮索引。
void blockyDigitalWrite::updateFrame(){
    x = blockyStartX + getBlockyLength(index);
    btn1->x = x;
    static_cast<blockyDeleteButton*>(btn1)->blockIndex = index;
}

// 函数功能: 执行数字输出积木动作。
// 物理意义: 对目标板指定IO下发数字电平。
void blockyDigitalWrite::execute(){
    BCPpinMode(targetMac, io, digitalWriteMode);
    BCPdigitalWrite(targetMac, io, value);
}

// 函数功能: 构造数字输出积木并生成标签。
blockyDigitalWrite::blockyDigitalWrite(uint8_t index, ioDefine io, int8_t value){
    this->index = index;
    this->io = io;
    this->value = value;
    width = 70;
    x = blockyStartX + getBlockyLength(index);
    snprintf(label, sizeof(label), "D%s=%s", ioToText(io), value ? "H" : "L");
    btn1 = new blockyDeleteButton(label, x, 10, this->index, width, 100, TFT_GREEN, TFT_BLACK);
}

// 函数功能: 析构数字输出积木。
blockyDigitalWrite::~blockyDigitalWrite(){
    delete btn1;
}

// 函数功能: 更新模拟积木位置并同步删除按钮索引。
void blockyAnalogWrite::updateFrame(){
    x = blockyStartX + getBlockyLength(index);
    btn1->x = x;
    static_cast<blockyDeleteButton*>(btn1)->blockIndex = index;
}

// 函数功能: 执行模拟输出积木动作。
// 算法: 将百分比占空比映射到8位PWM值(0~255)。
void blockyAnalogWrite::execute(){
    BCPpinMode(targetMac, io, analogWriteMode);
    int pwm = (dutyPercent * 255) / 100;
    BCPanalogWrite(targetMac, io, pwm);
}

// 函数功能: 构造模拟输出积木并生成标签。
blockyAnalogWrite::blockyAnalogWrite(uint8_t index, ioDefine io, int dutyPercent){
    this->index = index;
    this->io = io;
    this->dutyPercent = dutyPercent;
    width = 90;
    x = blockyStartX + getBlockyLength(index);
    snprintf(label, sizeof(label), "A%s=%d%%", ioToText(io), dutyPercent);
    btn1 = new blockyDeleteButton(label, x, 10, this->index, width, 100, TFT_ORANGE, TFT_BLACK);
}

// 函数功能: 析构模拟输出积木。
blockyAnalogWrite::~blockyAnalogWrite(){
    delete btn1;
}

// 函数功能: 顺序执行用户搭建的积木程序。
// 输入/输出: 无。
// 算法说明: 跳过索引0的Start积木，后续逐块执行并插入1s间隔。
void runBlockyProgram(){
    for(size_t i = 1; i < blockyPool.size(); i++){
        blockyPool[i]->execute();
        delay(1000);
    }
}

// 函数功能: 按滑条位置计算视口偏移，并刷新全部积木框架。
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

// 函数功能: 视图左移。
// 算法: 根据blockyMoveMode决定移动解释为“砖块”还是“视野”。
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

// 函数功能: 视图右移。
// 算法: 与blockyMoveLeft对称，并同步更新滑条百分比。
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

// 函数功能: 初始化Blocky模块。
// 输入/输出: 无。
// 语句组功能:
// 1) 初始化BCP并配对目标从机；
// 2) 创建主编辑页面；
// 3) 创建添加积木页面。
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

// 函数功能: 切换渲染到“添加积木”页面。
void switchToBlockyProgAdd(){
    renderActivityPtr = getActivity("blockyProgAdd");
    uiRender();
}

// 函数功能: 切换渲染到“主编辑”页面。
void switchToBlockyProgMain(){
    renderActivityPtr = getActivity("blockyProgMain");
    uiRender();
}


// 函数功能: 计算积木序列长度。
// 输入: index -> 统计到index前一个积木。
// 输出: 总宽度像素值，用于布局与滚动边界判断。
int16_t getBlockyLength(uint8_t index){
    int16_t length=0;
    for(uint8_t i=0; i<index; i++){
        length+=blockyPool[i]->width;
    }
    return length;
}