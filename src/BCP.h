//这是Board Communication Package，专为esp-now的板级通讯设计

#define isMaster 1 //是否是主机，只有主机可以发送配对
#define updateIoMsgInterval 100 //ms，更新ioMsg的间隔
#ifndef BCP_H
#define BCP_H
    #include <Arduino.h>
    #include <esp_now.h>
    #include <WiFi.h>
    #include <vector>
    

    extern uint8_t masterMac[6];
    extern const uint8_t nullMac[6];

    enum ioDefine {//引脚定义
        IO1 = 1,
        IO2 = 2,
        IO19 = 19,
        IO20 = 20,
        IO21 = 21,
        IO47 = 47,
        IO48 = 48,
        IO45 = 45,
        IO38 = 38,
        IO39 = 39,
    };
    enum ioMode {//引脚模式
        digitalReadMode = 0,
        digitalWriteMode = 1,
        analogReadMode = 2,
        analogWriteMode = 3,

    };
    struct ioMsgPack {
        int id;
        uint8_t mac[6];
        ioDefine ioDef[10];//引脚定义
        ioMode pinMode[10];//引脚模式
        int analogReadArray[10];//模拟量数据
        int8_t digitalReadArray[10];//数字量数据
        int analogWriteArray[10];//模拟量输出
        int8_t digitalWriteArray[10];//数字量输出

    };

    void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
    void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len);
    bool pairDevice(uint8_t* targetMac);
    #if isMaster == 0
        extern ioMsgPack ioMsg;//从机只需要单个数据包
    #elif isMaster == 1
        void sendMasterMsg(uint8_t* targetMac);
        ioMsgPack* getIoMsgPtr(uint8_t* targetMac);
        extern std::vector<ioMsgPack> ioMsgPool;

        bool BCPpinMode(uint8_t* targetMac, ioDefine io, ioMode mode);
        bool BCPdigitalWrite(uint8_t* targetMac, ioDefine io, int8_t value);
        int8_t BCPdigitalRead(uint8_t* targetMac, ioDefine io);
        bool BCPanalogWrite(uint8_t* targetMac, ioDefine io, int value);
        int BCPanalogRead(uint8_t* targetMac, ioDefine io);
    #endif

    void sendIoMsg(ioMsgPack* ioMsg);
    void initBCP();

    void updateIoMsg(void* parameter);
    void initIoMsg();
#endif