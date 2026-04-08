#include "BCP.h"
#include <esp_wifi.h>

// 主机MAC缓存：从机收到握手帧后更新，用于后续状态回传。
uint8_t masterMac[6]={0x00,0x00,0x00,0x00,0x00,0x00};//主机mac地址，初始为全0
const uint8_t nullMac[6]={0x00,0x00,0x00,0x00,0x00,0x00};
//////////////////////主从机不同的全局变量列表////////////////////////
#if isMaster == 0
    ioMsgPack ioMsg={
        .id = 0,
        .mac = {0x00,0x00,0x00,0x00,0x00,0x00},
        .ioDef = {IO1, IO2, IO19, IO20, IO21, IO47, IO48, IO45, IO38, IO39},
        .pinMode = {digitalReadMode, digitalReadMode, digitalReadMode, digitalReadMode, digitalReadMode, digitalReadMode, digitalReadMode, digitalReadMode, digitalReadMode, digitalReadMode},
        .analogReadArray = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        .digitalReadArray = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        .analogWriteArray = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        .digitalWriteArray = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    };//从机只保存一份数据
#elif isMaster == 1
    std::vector<ioMsgPack> ioMsgPool;
#endif
// 函数功能: ESP-NOW发送结果回调。
// 输入参数:
// mac_addr -> 目标设备MAC地址（物理意义: 这一帧发往哪块板）。
// status   -> 底层发送状态（物理意义: 无线链路是否确认成功）。
// 输出参数: 无（通过串口输出调试信息）。
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // 这里可以添加发送结果的处理逻辑
    Serial.printf("ESP_NOW发送成功, 状态: %s\n", status == ESP_NOW_SEND_SUCCESS ? "成功" : "失败");
}

// 函数功能: ESP-NOW接收回调，解析握手帧/IO数据帧并同步状态。
// 输入参数:
// mac  -> 实际发送方MAC地址（物理意义: 数据来源设备）。
// data -> 接收字节流，data[0]是帧类型。
// len  -> 接收字节数。
// 输出参数: 无（更新全局状态池）。
// 算法说明:
// 1) data[0]==0x00: 握手帧；从机记录主机MAC并建立peer。
// 2) data[0]==0x01: IO帧；主机更新采样值，从机更新控制指令。
void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
    Serial.printf("ESP_NOW收到数据, 长度: %d\n", len);
    if(data[0]== 0x00){
        #if isMaster == 0
            if(len != 7)return;//握手包长度固定为7字节
            memcpy(masterMac, data+1, 6);
            memcpy(&ioMsg.mac, data+1, 6);

            // 语句组功能: 从机将主机写入peer表。
            // 实现思路: ESP-NOW发送到某目标前必须先add_peer，否则回传失败。
            esp_now_peer_info_t peerInfo;
            memset(&peerInfo, 0, sizeof(peerInfo));
            memcpy(peerInfo.peer_addr, masterMac, 6);
            peerInfo.channel = 0;
            peerInfo.encrypt = false;
            if (!esp_now_is_peer_exist(masterMac)) {
                esp_err_t addResult = esp_now_add_peer(&peerInfo);
                Serial.printf("从机添加主机peer: %s\n", addResult == ESP_OK ? "成功" : "失败");
            }
            return;
        #endif
        }
    if(data[0] == 0x01){
        if(len != sizeof(ioMsgPack)+1)return;//数据长度不匹配，丢弃
        ioMsgPack received;
        memcpy(&received, data+1, sizeof(received));
        #if isMaster == 1
            // 主机端以回调参数mac为准：它是实际发送方(从机)MAC。
            memcpy(received.mac, mac, 6);
            // 语句组功能: 查找并增量更新对应从机的采样缓存。
            // 实现思路: 主机控制参数以本地为准，只覆写反馈数组避免误覆盖命令。
            for(auto & msg : ioMsgPool){
                if(memcmp(msg.mac, received.mac, 6) == 0){
                    //找到对应mac地址，更新数据
                    //主机只接受反馈数据
                    memcpy(&msg.analogReadArray, &received.analogReadArray, sizeof(received.analogReadArray));
                    memcpy(&msg.digitalReadArray, &received.digitalReadArray, sizeof(received.digitalReadArray));
                    return;
                }
            }
            ioMsgPool.push_back(received);
        #elif isMaster == 0
        // 语句组功能: 从机仅接收控制配置，不使用主机回传采样值。
        // 实现思路: 按字段拷贝模式与输出数组，让周期任务统一执行硬件读写。
        memcpy(&ioMsg.id, &received.id, sizeof(received.id));
        memcpy(&ioMsg.ioDef, &received.ioDef, sizeof(received.ioDef));
        memcpy(&ioMsg.pinMode, &received.pinMode, sizeof(received.pinMode));
        memcpy(&ioMsg.analogWriteArray, &received.analogWriteArray, sizeof(received.analogWriteArray));
        memcpy(&ioMsg.digitalWriteArray, &received.digitalWriteArray, sizeof(received.digitalWriteArray));
        #endif
    }
}
#if isMaster == 1
    // 函数功能: 主机发起配对，必要时先添加peer再发送握手。
    // 输入参数: targetMac -> 目标从机MAC地址。
    // 输出参数: 返回配对流程是否触发成功。
    // 算法说明: 若peer已存在直接握手；否则add_peer成功后再握手。
    bool pairDevice(uint8_t* targetMac) {
        if (esp_now_is_peer_exist(targetMac)) {
            sendMasterMsg(targetMac);
            return true;
        }

        esp_now_peer_info_t peerInfo;
        memset(&peerInfo, 0, sizeof(peerInfo));
        
        memcpy(peerInfo.peer_addr, targetMac, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        
        if (esp_now_add_peer(&peerInfo) == ESP_OK){
            sendMasterMsg(targetMac);
            Serial.println("配对成功");
            return true;
        }
        Serial.println("配对失败");
        return false;
    }
    // 函数功能: 根据MAC在主机消息池中查找对应设备状态包。
    // 输入参数: targetMac -> 目标从机MAC地址。
    // 输出参数: ioMsgPack*，不存在时为nullptr。
    ioMsgPack* getIoMsgPtr(uint8_t* targetMac){
        for(auto & msg : ioMsgPool){
            if(memcmp(msg.mac, targetMac, 6) == 0){
                return &msg;
            }
        }
        return nullptr;

    }
#elif isMaster == 0
    // 从机侧不主动配对，返回false用于上层区分角色行为。
    bool pairDevice(uint8_t* targetMac) {
        return false;
    }
#endif

// 4. 发送消息 - 最简单的版本

#if isMaster == 1
    // 函数功能: 发送主机握手包。
    // 输入参数: targetMac -> 从机MAC地址。
    // 输出参数: 无。
    // 算法说明: 帧格式为[0x00][主机6字节MAC]。
    void sendMasterMsg(uint8_t* targetMac){
        uint8_t sendPack[7];
        sendPack[0] = 0x00;// 消息类型标识,0是主机配对消息
        // 发送主机真实6字节MAC（不能用字符串形式"AA:BB:CC:DD:EE:FF"）
        uint8_t localMac[6];
        esp_wifi_get_mac(WIFI_IF_STA, localMac);
        memcpy(sendPack+1, localMac, 6);
        esp_now_send(targetMac, sendPack, sizeof(sendPack));
    }
#endif
// 函数功能: 发送IO控制/状态包。
// 输入参数: ioMsg -> 待发送数据包，ioMsg->mac为目标地址。
// 输出参数: 无（失败时串口打印错误码与目标地址）。
// 算法说明: 在负载头部插入类型字节0x01，后接ioMsgPack二进制内容。
void sendIoMsg(ioMsgPack* ioMsg) {
    uint8_t sendPack[sizeof(*ioMsg)+1];
    sendPack[0] = 0x01; // 消息类型标识,1是ioMsgPack
    memcpy(sendPack+1, ioMsg, sizeof(*ioMsg));
    esp_err_t result = esp_now_send(ioMsg->mac, sendPack, sizeof(sendPack));
    if(result != ESP_OK){
        Serial.printf("ESP_NOW发送调用失败, err=%d(%s), target=%02X:%02X:%02X:%02X:%02X:%02X\n",
            result,
            esp_err_to_name(result),
            ioMsg->mac[0], ioMsg->mac[1], ioMsg->mac[2], ioMsg->mac[3], ioMsg->mac[4], ioMsg->mac[5]);
    }
}

// 函数功能: 初始化通信环境。
// 输入/输出: 无。
// 语句组功能: 配置STA模式、初始化ESP-NOW、注册收发回调。
void initBCP(){
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    Serial.print("本机MAC: ");
    Serial.println(WiFi.macAddress());
    // 初始化ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW初始化失败");
        return;
    }
    // 注册回调函数
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);
}

// 函数功能: FreeRTOS周期任务，驱动主从机IO同步。
// 输入参数: parameter -> 任务参数，当前未使用。
// 输出参数: 无（无限循环）。
// 算法说明:
// 从机: 根据pinMode执行本地读写并上报。
// 主机: 轮询消息池向每个从机下发当前控制指令。
void updateIoMsg(void* parameter){
    while(1){
        #if isMaster == 0
            if(memcmp(&ioMsg.mac, &nullMac, 6)==0){
               //还没有和主机配对    
            }else{
                // 语句组功能: 遍历10个逻辑槽位，统一处理模式切换和IO读写。
                // 实现思路: 将远端配置转成本地硬件API调用，便于协议与硬件层解耦。
                for(int i = 0; i < 10; i++){
                    if(ioMsg.pinMode[i] == digitalWriteMode){
                        pinMode(ioMsg.ioDef[i], OUTPUT);
                        digitalWrite(ioMsg.ioDef[i], ioMsg.digitalWriteArray[i]);
                    }
                    else if(ioMsg.pinMode[i] == analogWriteMode){
                        pinMode(ioMsg.ioDef[i], OUTPUT);
                        analogWrite(ioMsg.ioDef[i], ioMsg.analogWriteArray[i]);
                    }
                    else if (ioMsg.pinMode[i] == digitalReadMode)
                    {
                        pinMode(ioMsg.ioDef[i], INPUT);
                        ioMsg.digitalReadArray[i] = digitalRead(ioMsg.ioDef[i]);
                    }
                    else if (ioMsg.pinMode[i] == analogReadMode)
                    {
                        pinMode(ioMsg.ioDef[i], INPUT);
                        ioMsg.analogReadArray[i] = analogRead(ioMsg.ioDef[i]);
                    }
                }
                sendIoMsg(&ioMsg);
            }
        #elif isMaster == 1
            // 语句组功能: 主机逐设备下发当前缓存控制帧。
            for(auto & msg : ioMsgPool){
                sendIoMsg(&msg);
            }
        #endif
        vTaskDelay(updateIoMsgInterval / portTICK_PERIOD_MS);
        //Serial.println("更新完成");
    }
};
void initIoMsg(){
    // 语句组功能: 创建后台任务，周期执行updateIoMsg。
    xTaskCreate(updateIoMsg, "updateIoMsg", 4096, NULL, 1, NULL);
}
#if isMaster == 1
        // 函数功能: 查找指定物理IO在消息包中的槽位索引。
        // 输入参数: msg -> 设备消息包；io -> 目标物理引脚。
        // 输出参数: 返回[0,9]有效索引，未找到返回-1。
        static int findIoSlot(const ioMsgPack* msg, ioDefine io){
            for(int i = 0; i < 10; i++){
                if(msg->ioDef[i] == io){
                    return i;
                }
            }
            return -1;
        }
    // 函数功能: 写入远端引脚模式缓存。
    // 输入参数: targetMac/io/mode。
    // 输出参数: bool，表示是否写入成功。
        bool BCPpinMode(uint8_t* targetMac, ioDefine io, ioMode mode){
            ioMsgPack *msg = getIoMsgPtr(targetMac);
            if(msg != nullptr){
                int slot = findIoSlot(msg, io);
                if(slot < 0) return false;
                msg->pinMode[slot] = mode;
                return true;
            }else{
                return false;
            }
        }
        // 函数功能: 写入远端数字输出缓存。
        // 输入参数: value为逻辑电平(0/1)。
        // 输出参数: bool，表示是否写入成功。
        bool BCPdigitalWrite(uint8_t* targetMac, ioDefine io, int8_t value){
            ioMsgPack *msg = getIoMsgPtr(targetMac);
            if(msg != nullptr){
                int slot = findIoSlot(msg, io);
                if(slot < 0) return false;
                msg->digitalWriteArray[slot] = value;
                return true;
            }else{
                return false;
            }
        }
        // 函数功能: 读取远端数字输入缓存。
        // 输出参数: 0/1有效，-1表示目标或引脚无效。
        int8_t BCPdigitalRead(uint8_t* targetMac, ioDefine io){
            ioMsgPack *msg = getIoMsgPtr(targetMac);
            if(msg != nullptr){
                int slot = findIoSlot(msg, io);
                if(slot < 0) return -1;
                return msg->digitalReadArray[slot];
            }else{
                return -1;
            }
        }
        // 函数功能: 写入远端模拟输出缓存。
        // 输入参数: value为模拟控制量（常用于PWM）。
        // 输出参数: bool，表示是否写入成功。
        bool BCPanalogWrite(uint8_t* targetMac, ioDefine io, int value){
            ioMsgPack *msg = getIoMsgPtr(targetMac);
            if(msg != nullptr){
                int slot = findIoSlot(msg, io);
                if(slot < 0) return false;
                msg->analogWriteArray[slot] = value;
                return true;
            }else{
                return false;
            }
        }
        // 函数功能: 读取远端模拟输入缓存。
        // 输出参数: ADC值，-1表示目标或引脚无效。
        int BCPanalogRead(uint8_t* targetMac, ioDefine io){
            ioMsgPack *msg = getIoMsgPtr(targetMac);
            if(msg != nullptr){
                int slot = findIoSlot(msg, io);
                if(slot < 0) return -1;
                return msg->analogReadArray[slot];
            }else{
                return -1;
            }
        }
#endif