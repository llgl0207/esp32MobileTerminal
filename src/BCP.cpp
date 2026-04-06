#include "BCP.h"
#include <esp_wifi.h>

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
// 1. 发送回调函数 - 最简单的版本
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // 这里可以添加发送结果的处理逻辑
    Serial.printf("ESP_NOW发送成功, 状态: %s\n", status == ESP_NOW_SEND_SUCCESS ? "成功" : "失败");
}

// 2. 接收回调函数 - 最简单的版本
void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
    Serial.printf("ESP_NOW收到数据, 长度: %d\n", len);
    if(data[0]== 0x00){
        #if isMaster == 0
            if(len != 7)return;//握手包长度固定为7字节
            memcpy(masterMac, data+1, 6);
            memcpy(&ioMsg.mac, data+1, 6);

            // 从机收到主机握手后，必须把主机加入peer，才能向主机发送状态/回包。
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
            //遍历ioMsgPool查找有无对应的mac地址
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
        //从机只接收控制指令
        memcpy(&ioMsg.id, &received.id, sizeof(received.id));
        memcpy(&ioMsg.ioDef, &received.ioDef, sizeof(received.ioDef));
        memcpy(&ioMsg.pinMode, &received.pinMode, sizeof(received.pinMode));
        memcpy(&ioMsg.analogWriteArray, &received.analogWriteArray, sizeof(received.analogWriteArray));
        memcpy(&ioMsg.digitalWriteArray, &received.digitalWriteArray, sizeof(received.digitalWriteArray));
        #endif
    }
}
#if isMaster == 1
    //主机才能发起配对
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
    ioMsgPack* getIoMsgPtr(uint8_t* targetMac){
        for(auto & msg : ioMsgPool){
            if(memcmp(msg.mac, targetMac, 6) == 0){
                return &msg;
            }
        }
        return nullptr;

    }
#elif isMaster == 0
    bool pairDevice(uint8_t* targetMac) {
        return false;
    }
#endif

// 4. 发送消息 - 最简单的版本

#if isMaster == 1
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

void updateIoMsg(void* parameter){
    while(1){
        #if isMaster == 0
            if(memcmp(&ioMsg.mac, &nullMac, 6)==0){
               //还没有和主机配对    
            }else{
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
            for(auto & msg : ioMsgPool){
                sendIoMsg(&msg);
            }
        #endif
        vTaskDelay(updateIoMsgInterval / portTICK_PERIOD_MS);
        //Serial.println("更新完成");
    }
};
void initIoMsg(){
    xTaskCreate(updateIoMsg, "updateIoMsg", 4096, NULL, 1, NULL);
}
#if isMaster == 1
        static int findIoSlot(const ioMsgPack* msg, ioDefine io){
            for(int i = 0; i < 10; i++){
                if(msg->ioDef[i] == io){
                    return i;
                }
            }
            return -1;
        }

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