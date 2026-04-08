//这是Board Communication Package，专为esp-now的板级通讯设计
//当前BCP采用星型网络拓扑结构，当前只有同步IO信息的功能，让远程控制IO变得像读写寄存器一样简单，以便调用

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
        int id;                     // 数据包编号，可用于上层业务区分不同控制帧
        uint8_t mac[6];             // 目标设备MAC地址（主机端）或本机MAC地址（从机端）
        ioDefine ioDef[10];         // 逻辑槽位绑定到的物理IO编号
        ioMode pinMode[10];         // 每个槽位当前工作模式（读/写，数字/模拟）
        int analogReadArray[10];    // 从机采样得到的模拟输入值（ADC原始值）
        int8_t digitalReadArray[10];// 从机采样得到的数字输入值（0/1）
        int analogWriteArray[10];   // 主机下发到从机的模拟输出值（如PWM占空比）
        int8_t digitalWriteArray[10];// 主机下发到从机的数字输出值（0/1）

    };

    // ESP-NOW发送回调。
    // 输入: mac_addr 为目标设备MAC；status 为发送链路状态。
    // 输出: 无返回值，通过串口输出发送结果。
    // 物理意义: 表示无线链路一次帧发送是否被底层确认。
    void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

    // ESP-NOW接收回调。
    // 输入: mac 为发送方MAC；data 为原始负载；len 为负载字节数。
    // 输出: 无返回值，内部更新主/从机状态池。
    // 算法: 通过data[0]区分握手帧和IO数据帧，并按角色执行不同同步逻辑。
    void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len);

    // 发起或确认配对。
    // 输入: targetMac 为目标板卡6字节MAC地址。
    // 输出: true 表示配对流程触发成功，false 表示失败/不支持。
    // 物理意义: 在ESP-NOW点对点通信前建立peer关系。
    bool pairDevice(uint8_t* targetMac);
    #if isMaster == 0
        extern ioMsgPack ioMsg;//从机只需要单个数据包
    #elif isMaster == 1
        // 发送主机握手帧（类型0x00）。
        // 输入: targetMac 为从机MAC。
        // 输出: 无，异步发送。
        void sendMasterMsg(uint8_t* targetMac);

        // 查询指定从机对应的IO状态包。
        // 输入: targetMac 为从机MAC。
        // 输出: 返回该从机状态包指针，未找到返回nullptr。
        ioMsgPack* getIoMsgPtr(uint8_t* targetMac);
        extern std::vector<ioMsgPack> ioMsgPool;

        // 设置远端某个IO槽位的模式。
        // 输入: targetMac 为目标从机，io 为物理引脚，mode 为工作模式。
        // 输出: true 表示写入成功，false 表示目标或引脚不存在。
        bool BCPpinMode(uint8_t* targetMac, ioDefine io, ioMode mode);

        // 设置远端数字输出电平。
        // 输入: targetMac 为目标从机，io 为物理引脚，value 为0/1。
        // 输出: true 表示写入成功。
        bool BCPdigitalWrite(uint8_t* targetMac, ioDefine io, int8_t value);

        // 读取远端数字输入缓存值。
        // 输入: targetMac 为目标从机，io 为物理引脚。
        // 输出: 返回0/1，失败返回-1。
        int8_t BCPdigitalRead(uint8_t* targetMac, ioDefine io);

        // 设置远端模拟输出值。
        // 输入: targetMac 为目标从机，io 为物理引脚，value 为模拟输出量。
        // 输出: true 表示写入成功。
        bool BCPanalogWrite(uint8_t* targetMac, ioDefine io, int value);

        // 读取远端模拟输入缓存值。
        // 输入: targetMac 为目标从机，io 为物理引脚。
        // 输出: ADC缓存值，失败返回-1。
        int BCPanalogRead(uint8_t* targetMac, ioDefine io);
    #endif

    // 发送IO数据帧（类型0x01）。
    // 输入: ioMsg 指向待发送的控制/反馈数据包。
    // 输出: 无返回值，失败时输出错误信息。
    void sendIoMsg(ioMsgPack* ioMsg);

    // 初始化BCP通信栈。
    // 输入: 无。
    // 输出: 无，内部完成WiFi STA和ESP-NOW初始化及回调注册。
    void initBCP();

    // 周期任务函数。
    // 输入: parameter 为FreeRTOS任务参数（当前未使用）。
    // 输出: 无返回值，循环执行IO采样/下发。
    // 算法: 从机执行IO读写并回传，主机遍历消息池下发控制帧。
    void updateIoMsg(void* parameter);

    // 创建IO周期任务。
    // 输入: 无。
    // 输出: 无，任务创建后后台运行。
    void initIoMsg();
#endif