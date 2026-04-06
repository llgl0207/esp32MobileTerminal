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
        BLOCKY_CREATE_IFELSE
    };

    class blockyBase{
        public:
        int16_t width;
        int16_t x;
        int16_t y;
        uint8_t index;
        virtual ~blockyBase(){}
        virtual void updateFrame()=0;
        virtual bool isIfElse(){return false;}
        virtual bool isStart(){return false;}
    };

    class blockyStart : public blockyBase{
        public:
        uiButton* btn1;
        void updateFrame();
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
        blockyDelay(double delayValueX100, uint8_t index);
        ~blockyDelay();
    };

    class nestedBlockBase{
        public:
        int16_t width;
        virtual ~nestedBlockBase(){}
        virtual void updateFrame(int16_t startX, int16_t startY)=0;
    };

    class nestedDelayBlock : public nestedBlockBase{
        public:
        uiButton* btn1;
        double delayValueX100;
        char textBuffer[24];
        void updateFrame(int16_t startX, int16_t startY);
        nestedDelayBlock(double delayValueX100);
        ~nestedDelayBlock();
    };

    class blockyIfElse : public blockyBase{
        public:
        uiButton* ifTagBtn;
        uiButton* elseTagBtn;
        std::vector<nestedBlockBase*> ifBlockPool;
        std::vector<nestedBlockBase*> elseBlockPool;
        int16_t getNestedLength(const std::vector<nestedBlockBase*>& pool);
        void refreshWidthFromNested();
        void updateFrame();
        bool isIfElse(){return true;}
        blockyIfElse(uint8_t index);
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