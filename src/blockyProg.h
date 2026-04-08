#ifndef BLOCKYPROG_H
#define BLOCKYPROG_H 
    #include "UI.h"
    #include "BCP.h"
    extern uint8_t targetMac[6];
    #define blockyHeight 100
    #define blockyMoveMode 1 //定义左右移动键究竟如何移动//0:移动的方向是砖块移动的方向 1:移动的方向是视野移动的方向

    // 初始化Blocky编程页面与通信通道。
    // 输入/输出: 无。
    // 功能: 建立ESP-NOW配对、创建主界面/添加界面、初始化基础积木。
    void blockyProgInit();

    // Blocky页面管理回调。
    // 输入/输出: 无。
    // 功能: 根据滚动状态更新所有积木按钮的屏幕位置。
    void blockyMgr();
    
    extern uiSlider* blockyControlSlider;
    extern int16_t blockyStartX;
    class blockyBase{
        public:
        int16_t width;  // 当前积木在时间轴上的显示宽度（像素）
        int16_t x;      // 当前积木左上角X坐标（像素）
        uint8_t index;  // 当前积木在blockyPool中的顺序索引
        virtual ~blockyBase() = default;
        // 刷新积木框架位置。
        // 输入/输出: 无。功能: 根据index和滚动偏移更新x与按钮位置。
        virtual void updateFrame()=0;

        // 执行积木逻辑。
        // 输入/输出: 无。功能: 将积木语义转换为具体IO控制动作。
        virtual void execute()=0;
    };
    class blockyStart : public blockyBase{
        public:
        uiButton* btn1;
        void updateFrame();
        void execute();
        ~blockyStart();
        // 输入: index 为积木在程序中的顺序。
        // 功能: 创建Start积木按钮。
        blockyStart(uint8_t index);
    };
    class blockyDigitalWrite : public blockyBase{
        public:
        uiButton* btn1;
        ioDefine io;
        int8_t value;
        char label[24];
        void updateFrame();
        void execute();
        ~blockyDigitalWrite();
        // 输入: index 为顺序；io 为目标引脚；value 为数字电平(0/1)。
        // 功能: 构造数字输出积木。
        blockyDigitalWrite(uint8_t index, ioDefine io, int8_t value);
    };
    class blockyAnalogWrite : public blockyBase{
        public:
        uiButton* btn1;
        ioDefine io;
        int dutyPercent;
        char label[24];
        void updateFrame();
        void execute();
        ~blockyAnalogWrite();
        // 输入: index 为顺序；io 为目标引脚；dutyPercent 为占空比百分比(0~100)。
        // 功能: 构造模拟输出积木。
        blockyAnalogWrite(uint8_t index, ioDefine io, int dutyPercent);
    };
    extern std::vector<blockyBase*> blockyPool;

    // 计算从第0块到index-1块的总宽度。
    // 输入: index 为截止索引（默认到当前积木池末尾）。
    // 输出: 总长度（像素），用于布局与滚动。
    int16_t getBlockyLength(uint8_t index = blockyPool.size());

    // 视图向左滚动。
    // 输入/输出: 无。功能: 更新blockyStartX和控制滑条比例。
    void blockyMoveLeft();

    // 视图向右滚动。
    // 输入/输出: 无。功能: 更新blockyStartX和控制滑条比例。
    void blockyMoveRight();

    // 切换到“添加积木”页面。
    void switchToBlockyProgAdd();

    // 切换到“主编辑”页面。
    void switchToBlockyProgMain();

    // 顺序执行积木程序。
    // 输入/输出: 无。算法: 跳过Start块，按索引依次执行每个积木动作。
    void runBlockyProgram();

#endif