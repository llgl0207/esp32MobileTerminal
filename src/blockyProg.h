#ifndef BLOCKYPROG_H
#define BLOCKYPROG_H 
    #include "UI.h"
    #include <vector>
    #include "BCP.h"
    extern uint8_t targetMac[6];
    #define blockyHeight 100
    void blockyProgInit();
    void blockyMgr();
    
    extern uiSlider* blockyControlSlider;
    extern int16_t blockyStartX;

    enum blockyCreateType {
        BLOCKY_CREATE_NONE = 0,
        BLOCKY_CREATE_DELAY,
        BLOCKY_CREATE_DIGITAL_OUT,
        BLOCKY_CREATE_ANALOG_OUT,
        BLOCKY_CREATE_IF_IO_HIGH,
        BLOCKY_CREATE_IF_IO_LOW,
        BLOCKY_CREATE_IF_ANALOG_GT,
        BLOCKY_CREATE_IF_ANALOG_LT,
        BLOCKY_CREATE_IO_READ,
        BLOCKY_CREATE_INPUT_NUM
    };

    enum blockyIfCondType {
        BLOCKY_IF_IO_HIGH = 0,
        BLOCKY_IF_IO_LOW,
        BLOCKY_IF_ANALOG_GT,
        BLOCKY_IF_ANALOG_LT
    };

    class blockyBase{
        public:
        int16_t width;
        int16_t x;
        int16_t y;
        uint8_t index;
        virtual ~blockyBase(){}
        virtual void updateFrame()=0;
        virtual void execute(){}
        virtual bool isIfElse(){return false;}
        virtual bool isStart(){return false;}
    };

    class blockyStart : public blockyBase{
        public:
        uiButton* btn1;
        void updateFrame();
        void execute(){}
        bool isStart(){return true;}
        blockyStart(uint8_t index);
        ~blockyStart();
    };

    class blockyDelay : public blockyBase{
        public:
        uiButton* btn1;
        double delayValueX100;
        char textBuffer[24];
        void updateFrame();
        void execute();
        blockyDelay(double delayValueX100, uint8_t index);
        ~blockyDelay();
    };

    class blockyDigitalWrite : public blockyBase{
        public:
        uiButton* btn1;
        ioDefine io;
        bool highLevel;
        char textBuffer[28];
        void updateFrame();
        void execute();
        blockyDigitalWrite(ioDefine io, bool highLevel, uint8_t index);
        ~blockyDigitalWrite();
    };

    class blockyAnalogWrite : public blockyBase{
        public:
        uiButton* btn1;
        ioDefine io;
        double dutyX100;
        char textBuffer[32];
        void updateFrame();
        void execute();
        blockyAnalogWrite(ioDefine io, double dutyX100, uint8_t index);
        ~blockyAnalogWrite();
    };

    class blockyDigitalRead : public blockyBase{
        public:
        uiButton* btn1;
        char textBuffer[24];
        void updateFrame();
        void execute();
        blockyDigitalRead(uint8_t index);
        ~blockyDigitalRead();
    };

    class blockyInputNum : public blockyBase{
        public:
        uiButton* btn1;
        char textBuffer[24];
        void updateFrame();
        void execute();
        blockyInputNum(uint8_t index);
        ~blockyInputNum();
    };

    class nestedBlockBase{
        public:
        int16_t width;
        virtual ~nestedBlockBase(){}
        virtual void updateFrame(int16_t startX, int16_t startY)=0;
        virtual void execute(){}
    };

    class nestedDelayBlock : public nestedBlockBase{
        public:
        uiButton* btn1;
        double delayValueX100;
        char textBuffer[24];
        void updateFrame(int16_t startX, int16_t startY);
        void execute();
        nestedDelayBlock(double delayValueX100);
        ~nestedDelayBlock();
    };

    class nestedDigitalWriteBlock : public nestedBlockBase{
        public:
        uiButton* btn1;
        ioDefine io;
        bool highLevel;
        char textBuffer[28];
        void updateFrame(int16_t startX, int16_t startY);
        void execute();
        nestedDigitalWriteBlock(ioDefine io, bool highLevel);
        ~nestedDigitalWriteBlock();
    };

    class nestedAnalogWriteBlock : public nestedBlockBase{
        public:
        uiButton* btn1;
        ioDefine io;
        double dutyX100;
        char textBuffer[32];
        void updateFrame(int16_t startX, int16_t startY);
        void execute();
        nestedAnalogWriteBlock(ioDefine io, double dutyX100);
        ~nestedAnalogWriteBlock();
    };

    class blockyIfElse : public blockyBase{
        public:
        uiButton* ifTagBtn;
        uiButton* elseTagBtn;
        blockyIfCondType condType;
        ioDefine condIo;
        int condValue;
        char ifTextBuffer[28];
        std::vector<nestedBlockBase*> ifBlockPool;
        std::vector<nestedBlockBase*> elseBlockPool;
        int16_t getNestedLength(const std::vector<nestedBlockBase*>& pool);
        void refreshWidthFromNested();
        void updateFrame();
        bool evalCond();
        void execute();
        bool isIfElse(){return true;}
        blockyIfElse(uint8_t index, blockyIfCondType condType, ioDefine condIo, int condValue = 0);
        ~blockyIfElse();
    };

    extern std::vector<blockyBase*> blockyPool;
    extern blockyCreateType pendingCreateType;

    int16_t getBlockyLength(uint8_t index = blockyPool.size());
    void blockyMoveLeft();
    void blockyMoveRight();
    void switchToBlockyProgAdd();
    void switchToBlockyProgMain();
    void blockyRuntimeStep();
    void blockyStartSelectAddType(blockyCreateType type);
    void blockyShowInsertSlots();
    void blockyConfirmInsert(uint8_t insertIndex);

#endif