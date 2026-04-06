#ifndef BLOCKYPROG_H
#define BLOCKYPROG_H 
    #include "UI.h"
    #include "BCP.h"
    extern uint8_t targetMac[6];
    #define blockyHeight 100
    void blockyProgInit();
    void blockyMgr();
    
    extern uiSlider* blockyControlSlider;
    extern int16_t blockyStartX;
    class blockyBase{
        public:
        int16_t width;
        int16_t x;
        uint8_t index;
        virtual ~blockyBase() = default;
        virtual void updateFrame()=0;
        virtual void execute()=0;
    };
    class blockyStart : public blockyBase{
        public:
        uiButton* btn1;
        void updateFrame();
        void execute();
        ~blockyStart();
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
        blockyAnalogWrite(uint8_t index, ioDefine io, int dutyPercent);
    };
    extern std::vector<blockyBase*> blockyPool;
    int16_t getBlockyLength(uint8_t index = blockyPool.size());
    void blockyMoveLeft();
    void blockyMoveRight();
    void switchToBlockyProgAdd();
    void switchToBlockyProgMain();
    void runBlockyProgram();

#endif