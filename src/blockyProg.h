#ifndef BLOCKYPROG_H
#define BLOCKYPROG_H 
    #include "UI.h"
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
        virtual void updateFrame()=0;
    };
    class blockyStart : public blockyBase{
        public:
        uiButton* btn1;
        void updateFrame();
        blockyStart(uint8_t index);
    };
    extern std::vector<blockyBase*> blockyPool;
    int16_t getBlockyLength(uint8_t index = blockyPool.size());
    void blockyMoveLeft();
    void blockyMoveRight();


#endif