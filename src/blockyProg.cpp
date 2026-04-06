#include "blockyProg.h"
#include "Arduino.h"
#include <cstdio>
//DC:B4:D9:21:59:88
uint8_t targetMac[6]={0xDC,0xB4,0xD9,0x21,0x59,0x88};

std::vector<blockyBase*> blockyPool;
uiSlider* blockyControlSlider;
int16_t blockyStartX = 10;
blockyCreateType pendingCreateType = BLOCKY_CREATE_NONE;

#define blockyMoveMode 1 // 0:移动的方向是砖块移动的方向 1:移动的方向是视野移动的方向

static std::vector<uiButton*> blockyInsertSlotBtns;
static std::vector<uiButton*> blockyNestedInsertSlotBtns;
static bool blockyIsSelectingInsertSlot = false;

static int16_t blockyPendingInsertIndex = -1;
static blockyIfElse* blockyPendingNestedOwner = nullptr;
static bool blockyPendingNestedToElse = false;
static int16_t blockyPendingNestedInsertIndex = -1;

static bool blockyDeleteRequested = false;
static bool blockyRunRequested = false;
static blockyBase* selectedMainBlock = nullptr;
static blockyIfElse* selectedNestedOwner = nullptr;
static nestedBlockBase* selectedNestedBlock = nullptr;
static bool selectedNestedToElse = false;
static uiButton* blockyDeleteBtn = nullptr;
static int blockyRuntimeNumInput = 0;
static ioDefine blockyPendingIo = IO1;
static ioDefine blockyPendingIfIo = IO1;
static bool blockyPendingDigitalHigh = false;
static double blockyPendingAnalogDutyX100 = 0.0;
static int blockyPendingIfCondValue = 0;

static bool isMainBlockAlive(blockyBase* ptr) {
    if (ptr == nullptr) {
        return false;
    }
    for (auto block : blockyPool) {
        if (block == ptr) {
            return true;
        }
    }
    return false;
}

static bool isNestedSelectionAlive() {
    if (selectedNestedOwner == nullptr || selectedNestedBlock == nullptr) {
        return false;
    }
    if (!isMainBlockAlive(selectedNestedOwner)) {
        return false;
    }
    std::vector<nestedBlockBase*>& pool = selectedNestedToElse ? selectedNestedOwner->elseBlockPool : selectedNestedOwner->ifBlockPool;
    for (auto nested : pool) {
        if (nested == selectedNestedBlock) {
            return true;
        }
    }
    return false;
}

static void clearSelection() {
    selectedMainBlock = nullptr;
    selectedNestedOwner = nullptr;
    selectedNestedBlock = nullptr;
}

static int16_t ifElseIfBaseX(blockyIfElse* owner) {
    return owner->x + 50;
}

static int16_t ifElseIfRenderLen(blockyIfElse* owner) {
    int16_t len = owner->getNestedLength(owner->ifBlockPool);
    if (len < 40) {
        len = 40;
    }
    return len;
}

static int16_t ifElseElseTagX(blockyIfElse* owner) {
    return ifElseIfBaseX(owner) + ifElseIfRenderLen(owner) + 6;
}

static int16_t ifElseElseBaseX(blockyIfElse* owner) {
    return ifElseElseTagX(owner) + 40 + 6;
}

static int16_t ifElseElseRenderLen(blockyIfElse* owner) {
    int16_t len = owner->getNestedLength(owner->elseBlockPool);
    if (len < 40) {
        len = 40;
    }
    return len;
}

static int16_t getNestedLengthPrefix(const std::vector<nestedBlockBase*>& pool, uint8_t index) {
    int16_t length = 0;
    for (uint8_t i = 0; i < index && i < pool.size(); i++) {
        length += pool[i]->width;
    }
    return length;
}

class blockyMainSelectBtn : public uiButton {
public:
    blockyBase* owner;
    blockyMainSelectBtn(blockyBase* owner, const char* text, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t bg, uint32_t fg)
        : uiButton(text, x, y, nullFunc, w, h, bg, fg), owner(owner) {}

protected:
    void endClick() {
        inverseColor();
        if (lastInTheBtn) {
            selectedMainBlock = owner;
            selectedNestedOwner = nullptr;
            selectedNestedBlock = nullptr;
        }
    }
};

class blockyStartRunBtn : public uiButton {
public:
    blockyStartRunBtn(const char* text, int16_t x, int16_t y, int16_t w, int16_t h)
        : uiButton(text, x, y, nullFunc, w, h, TFT_CYAN, TFT_BLACK) {}

protected:
    void endClick() {
        inverseColor();
        if (lastInTheBtn) {
            blockyRunRequested = true;
        }
    }
};

class blockyNestedSelectBtn : public uiButton {
public:
    blockyIfElse* ownerIfElse;
    nestedBlockBase* ownerNested;
    bool inElse;
    blockyNestedSelectBtn(blockyIfElse* ownerIfElse, nestedBlockBase* ownerNested, bool inElse,
                          const char* text, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t bg, uint32_t fg)
        : uiButton(text, x, y, nullFunc, w, h, bg, fg), ownerIfElse(ownerIfElse), ownerNested(ownerNested), inElse(inElse) {}

protected:
    void endClick() {
        inverseColor();
        if (lastInTheBtn) {
            selectedMainBlock = nullptr;
            selectedNestedOwner = ownerIfElse;
            selectedNestedBlock = ownerNested;
            selectedNestedToElse = inElse;
        }
    }
};

class blockyInsertSlotBtn : public uiButton {
public:
    uint8_t insertIndex;
    blockyInsertSlotBtn(uint8_t insertIndex, int16_t x, int16_t y)
        : uiButton("+", x, y, nullFunc, 20, 20, TFT_YELLOW, TFT_BLACK), insertIndex(insertIndex) {}

protected:
    void endClick() {
        inverseColor();
        if (lastInTheBtn) {
            blockyPendingInsertIndex = insertIndex;
        }
    }
};

class blockyNestedInsertSlotBtn : public uiButton {
public:
    blockyIfElse* owner;
    bool toElse;
    uint8_t insertIndex;
    blockyNestedInsertSlotBtn(blockyIfElse* owner, bool toElse, uint8_t insertIndex, int16_t x, int16_t y)
        : uiButton("+", x, y, nullFunc, 18, 18, TFT_ORANGE, TFT_BLACK), owner(owner), toElse(toElse), insertIndex(insertIndex) {}

protected:
    void endClick() {
        inverseColor();
        if (lastInTheBtn) {
            blockyPendingNestedOwner = owner;
            blockyPendingNestedToElse = toElse;
            blockyPendingNestedInsertIndex = insertIndex;
        }
    }
};

class blockyDeleteActionBtn : public uiButton {
public:
    blockyDeleteActionBtn(int16_t x, int16_t y)
        : uiButton("Delete", x, y, nullFunc, 90, 28, TFT_RED, TFT_WHITE) {
        // 触摸屏上按压时间常超过 200ms，放大阈值避免被误判成长按。
        clickTimeMax = 600;
    }

protected:
    void endClick() {
        inverseColor();
        if (lastInTheBtn) {
            blockyDeleteRequested = true;
        }
    }

    void endLongPress() {
        inverseColor();
        if (lastInTheBtn) {
            blockyDeleteRequested = true;
        }
    }
};

static void clearDeleteButton() {
    if (blockyDeleteBtn != nullptr) {
        delete blockyDeleteBtn;
        blockyDeleteBtn = nullptr;
    }
}

static void showDeleteButton() {
    uiActivity* mainActivity = getActivity("blockyProgMain");
    if (mainActivity == nullptr) {
        return;
    }

    uiActivity* tempControl = controlActivityPtr;
    controlActivityPtr = mainActivity;
    if (blockyDeleteBtn == nullptr) {
        blockyDeleteBtn = new blockyDeleteActionBtn(220, 170);
    }
    controlActivityPtr = tempControl;
}

static void clearInsertSlotButtons() {
    for (auto btn : blockyInsertSlotBtns) {
        delete btn;
    }
    blockyInsertSlotBtns.clear();

    for (auto btn : blockyNestedInsertSlotBtns) {
        delete btn;
    }
    blockyNestedInsertSlotBtns.clear();

    blockyIsSelectingInsertSlot = false;
    blockyPendingInsertIndex = -1;
    blockyPendingNestedOwner = nullptr;
    blockyPendingNestedInsertIndex = -1;
}

static void reindexBlockyPool() {
    for (uint8_t i = 0; i < blockyPool.size(); i++) {
        blockyPool[i]->index = i;
    }
}

static void ensureStartAtFront() {
    int16_t startPos = -1;
    for (uint8_t i = 0; i < blockyPool.size(); i++) {
        if (blockyPool[i]->isStart()) {
            startPos = i;
            break;
        }
    }

    if (startPos < 0) {
        blockyPool.insert(blockyPool.begin(), new blockyStart(0));
        reindexBlockyPool();
        return;
    }

    if (startPos > 0) {
        blockyBase* startPtr = blockyPool[startPos];
        blockyPool.erase(blockyPool.begin() + startPos);
        blockyPool.insert(blockyPool.begin(), startPtr);
        reindexBlockyPool();
    }
}

void blockyStart::updateFrame() {
    x = blockyStartX + getBlockyLength(index);
    y = 10;
    btn1->x = x;
    btn1->y = y;
}

blockyStart::blockyStart(uint8_t index) {
    this->index = index;
    width = 50;
    y = 10;
    x = blockyStartX + getBlockyLength(index);
    btn1 = new blockyStartRunBtn("Start", x, y, width, blockyHeight);
}

blockyStart::~blockyStart() {
    delete btn1;
}

void blockyDelay::updateFrame() {
    x = blockyStartX + getBlockyLength(index);
    y = 10;
    snprintf(textBuffer, sizeof(textBuffer), "Delay %.2f", delayValueX100 / 100.0f);
    btn1->text = textBuffer;
    btn1->x = x;
    btn1->y = y;
}

void blockyDelay::execute() {
    delay((uint32_t)(delayValueX100 * 10));
}

blockyDelay::blockyDelay(double delayValueX100, uint8_t index) {
    this->index = index;
    this->delayValueX100 = delayValueX100;
    width = 120;
    y = 10;
    x = blockyStartX + getBlockyLength(index);
    snprintf(textBuffer, sizeof(textBuffer), "Delay %.2f", delayValueX100 / 100.0f);
    btn1 = new blockyMainSelectBtn(this, textBuffer, x, y, width, blockyHeight, TFT_CYAN, TFT_BLACK);
}

blockyDelay::~blockyDelay() {
    delete btn1;
}

static const char* ioToText(ioDefine io) {
    switch (io) {
        case IO1: return "IO1";
        case IO2: return "IO2";
        case IO19: return "IO19";
        case IO20: return "IO20";
        case IO47: return "IO47";
        case IO48: return "IO48";
        default: return "IO?";
    }
}

void blockyDigitalWrite::updateFrame() {
    x = blockyStartX + getBlockyLength(index);
    y = 10;
    snprintf(textBuffer, sizeof(textBuffer), "%s=%d", ioToText(io), highLevel ? 1 : 0);
    btn1->text = textBuffer;
    btn1->x = x;
    btn1->y = y;
}

void blockyDigitalWrite::execute() {
    BCPpinMode(targetMac, io, digitalWriteMode);
    BCPdigitalWrite(targetMac, io, highLevel ? 1 : 0);
}

blockyDigitalWrite::blockyDigitalWrite(ioDefine io, bool highLevel, uint8_t index) {
    this->index = index;
    this->io = io;
    this->highLevel = highLevel;
    width = 105;
    y = 10;
    x = blockyStartX + getBlockyLength(index);
    snprintf(textBuffer, sizeof(textBuffer), "%s=%d", ioToText(io), highLevel ? 1 : 0);
    uint32_t bg = highLevel ? TFT_GREEN : TFT_ORANGE;
    btn1 = new blockyMainSelectBtn(this, textBuffer, x, y, width, blockyHeight, bg, TFT_BLACK);
}

blockyDigitalWrite::~blockyDigitalWrite() {
    delete btn1;
}

void blockyAnalogWrite::updateFrame() {
    x = blockyStartX + getBlockyLength(index);
    y = 10;
    snprintf(textBuffer, sizeof(textBuffer), "%s PWM %.0f%%", ioToText(io), dutyX100);
    btn1->text = textBuffer;
    btn1->x = x;
    btn1->y = y;
}

void blockyAnalogWrite::execute() {
    int value255 = (int)(dutyX100 * 2.55f);
    if (value255 < 0) value255 = 0;
    if (value255 > 255) value255 = 255;
    BCPpinMode(targetMac, io, analogWriteMode);
    BCPanalogWrite(targetMac, io, value255);
}

blockyAnalogWrite::blockyAnalogWrite(ioDefine io, double dutyX100, uint8_t index) {
    this->index = index;
    this->io = io;
    this->dutyX100 = dutyX100;
    width = 145;
    y = 10;
    x = blockyStartX + getBlockyLength(index);
    snprintf(textBuffer, sizeof(textBuffer), "%s PWM %.0f%%", ioToText(io), dutyX100);
    btn1 = new blockyMainSelectBtn(this, textBuffer, x, y, width, blockyHeight, TFT_MAGENTA, TFT_WHITE);
}

blockyAnalogWrite::~blockyAnalogWrite() {
    delete btn1;
}

void blockyDigitalRead::updateFrame() {
    x = blockyStartX + getBlockyLength(index);
    y = 10;
    snprintf(textBuffer, sizeof(textBuffer), "Read IO1");
    btn1->text = textBuffer;
    btn1->x = x;
    btn1->y = y;
}

void blockyDigitalRead::execute() {
    BCPpinMode(targetMac, IO1, digitalReadMode);
    int8_t value = BCPdigitalRead(targetMac, IO1);
    static char msg[40];
    snprintf(msg, sizeof(msg), "IO1 level: %d", value);
    popUp(msg);
}

blockyDigitalRead::blockyDigitalRead(uint8_t index) {
    this->index = index;
    width = 105;
    y = 10;
    x = blockyStartX + getBlockyLength(index);
    snprintf(textBuffer, sizeof(textBuffer), "Read IO1");
    btn1 = new blockyMainSelectBtn(this, textBuffer, x, y, width, blockyHeight, TFT_YELLOW, TFT_BLACK);
}

blockyDigitalRead::~blockyDigitalRead() {
    delete btn1;
}

void blockyInputNum::updateFrame() {
    x = blockyStartX + getBlockyLength(index);
    y = 10;
    snprintf(textBuffer, sizeof(textBuffer), "NumIn=%d", blockyRuntimeNumInput);
    btn1->text = textBuffer;
    btn1->x = x;
    btn1->y = y;
}

void blockyInputNum::execute() {
    double valueX100 = uiInputNumberSliderX100();
    blockyRuntimeNumInput = (valueX100 >= 50.0) ? 1 : 0;
}

blockyInputNum::blockyInputNum(uint8_t index) {
    this->index = index;
    width = 100;
    y = 10;
    x = blockyStartX + getBlockyLength(index);
    snprintf(textBuffer, sizeof(textBuffer), "NumIn=%d", blockyRuntimeNumInput);
    btn1 = new blockyMainSelectBtn(this, textBuffer, x, y, width, blockyHeight, TFT_LIGHTGREY, TFT_BLACK);
}

blockyInputNum::~blockyInputNum() {
    delete btn1;
}

void nestedDelayBlock::updateFrame(int16_t startX, int16_t startY) {
    snprintf(textBuffer, sizeof(textBuffer), "Dly %.1f", delayValueX100 / 100.0f);
    btn1->text = textBuffer;
    btn1->x = startX;
    btn1->y = startY;
}

void nestedDelayBlock::execute() {
    delay((uint32_t)(delayValueX100 * 10));
}

static void drawNestedWriteText(char* buffer, size_t bufferSize, ioDefine io, bool highLevel) {
    snprintf(buffer, bufferSize, "%s=%d", ioToText(io), highLevel ? 1 : 0);
}

nestedDelayBlock::nestedDelayBlock(double delayValueX100) {
    this->delayValueX100 = delayValueX100;
    width = 90;
    snprintf(textBuffer, sizeof(textBuffer), "Dly %.1f", delayValueX100 / 100.0f);
    // 嵌套块高度缩小10px，并保持与父块顶端对齐。
    btn1 = nullptr;
}

nestedDelayBlock::~nestedDelayBlock() {
    if (btn1 != nullptr) {
        delete btn1;
    }
}

void nestedDigitalWriteBlock::updateFrame(int16_t startX, int16_t startY) {
    drawNestedWriteText(textBuffer, sizeof(textBuffer), io, highLevel);
    btn1->text = textBuffer;
    btn1->x = startX;
    btn1->y = startY;
}

void nestedDigitalWriteBlock::execute() {
    BCPpinMode(targetMac, io, digitalWriteMode);
    BCPdigitalWrite(targetMac, io, highLevel ? 1 : 0);
}

nestedDigitalWriteBlock::nestedDigitalWriteBlock(ioDefine io, bool highLevel) {
    this->io = io;
    this->highLevel = highLevel;
    width = 100;
    drawNestedWriteText(textBuffer, sizeof(textBuffer), io, highLevel);
    btn1 = nullptr;
}

nestedDigitalWriteBlock::~nestedDigitalWriteBlock() {
    if (btn1 != nullptr) {
        delete btn1;
    }
}

void nestedAnalogWriteBlock::updateFrame(int16_t startX, int16_t startY) {
    snprintf(textBuffer, sizeof(textBuffer), "%s PWM %.0f%%", ioToText(io), dutyX100);
    btn1->text = textBuffer;
    btn1->x = startX;
    btn1->y = startY;
}

void nestedAnalogWriteBlock::execute() {
    int value255 = (int)(dutyX100 * 2.55f);
    if (value255 < 0) value255 = 0;
    if (value255 > 255) value255 = 255;
    BCPpinMode(targetMac, io, analogWriteMode);
    BCPanalogWrite(targetMac, io, value255);
}

nestedAnalogWriteBlock::nestedAnalogWriteBlock(ioDefine io, double dutyX100) {
    this->io = io;
    this->dutyX100 = dutyX100;
    width = 140;
    snprintf(textBuffer, sizeof(textBuffer), "%s PWM %.0f%%", ioToText(io), dutyX100);
    btn1 = nullptr;
}

nestedAnalogWriteBlock::~nestedAnalogWriteBlock() {
    if (btn1 != nullptr) {
        delete btn1;
    }
}

int16_t blockyIfElse::getNestedLength(const std::vector<nestedBlockBase*>& pool) {
    int16_t length = 0;
    for (auto nested : pool) {
        length += nested->width;
    }
    return length;
}

void blockyIfElse::refreshWidthFromNested() {
    width = 112 + ifElseIfRenderLen(this) + ifElseElseRenderLen(this);
}

void blockyIfElse::updateFrame() {
    refreshWidthFromNested();
    x = blockyStartX + getBlockyLength(index);
    y = 10;

    if (condType == BLOCKY_IF_IO_HIGH) {
        snprintf(ifTextBuffer, sizeof(ifTextBuffer), "%s=H", ioToText(condIo));
    } else if (condType == BLOCKY_IF_IO_LOW) {
        snprintf(ifTextBuffer, sizeof(ifTextBuffer), "%s=L", ioToText(condIo));
    } else if (condType == BLOCKY_IF_ANALOG_GT) {
        snprintf(ifTextBuffer, sizeof(ifTextBuffer), "%s>%d", ioToText(condIo), condValue);
    } else {
        snprintf(ifTextBuffer, sizeof(ifTextBuffer), "%s<%d", ioToText(condIo), condValue);
    }
    ifTagBtn->text = ifTextBuffer;

    ifTagBtn->x = x + 5;
    ifTagBtn->y = y;
    elseTagBtn->x = ifElseElseTagX(this);
    elseTagBtn->y = y;

    int16_t ifBaseX = ifElseIfBaseX(this);
    for (uint8_t i = 0; i < ifBlockPool.size(); i++) {
        int16_t nestedX = ifBaseX + getNestedLengthPrefix(ifBlockPool, i);
        ifBlockPool[i]->updateFrame(nestedX, y);
    }

    int16_t elseBaseX = ifElseElseBaseX(this);
    for (uint8_t i = 0; i < elseBlockPool.size(); i++) {
        int16_t nestedX = elseBaseX + getNestedLengthPrefix(elseBlockPool, i);
        elseBlockPool[i]->updateFrame(nestedX, y);
    }
}

bool blockyIfElse::evalCond() {
    if (condType == BLOCKY_IF_IO_HIGH || condType == BLOCKY_IF_IO_LOW) {
        BCPpinMode(targetMac, condIo, digitalReadMode);
        int8_t value = BCPdigitalRead(targetMac, condIo);
        if (condType == BLOCKY_IF_IO_HIGH) {
            return value == 1;
        }
        return value == 0;
    }

    BCPpinMode(targetMac, condIo, analogReadMode);
    int analogValue = BCPanalogRead(targetMac, condIo);
    if (condType == BLOCKY_IF_ANALOG_GT) {
        return analogValue > condValue;
    }
    return analogValue < condValue;
}

void blockyIfElse::execute() {
    const std::vector<nestedBlockBase*>& targetPool = evalCond() ? ifBlockPool : elseBlockPool;
    for (auto nested : targetPool) {
        nested->execute();
    }
}

blockyIfElse::blockyIfElse(uint8_t index, blockyIfCondType condType, ioDefine condIo, int condValue) {
    this->index = index;
    this->condType = condType;
    this->condIo = condIo;
    this->condValue = condValue;
    y = 10;
    ifTagBtn = new blockyMainSelectBtn(this, "IF", 0, y, 52, 24, TFT_BLUE, TFT_WHITE);
    elseTagBtn = new blockyMainSelectBtn(this, "ELSE", 0, y, 40, 24, TFT_DARKGREY, TFT_WHITE);
    refreshWidthFromNested();
}

blockyIfElse::~blockyIfElse() {
    for (auto nested : ifBlockPool) {
        delete nested;
    }
    ifBlockPool.clear();

    for (auto nested : elseBlockPool) {
        delete nested;
    }
    elseBlockPool.clear();

    delete ifTagBtn;
    delete elseTagBtn;
}

void blockyMgr() {
    if (getBlockyLength() > 320) {
        blockyStartX = -(getBlockyLength() - 320) * blockyControlSlider->percentage;
    } else {
        blockyStartX = 10;
    }

    for (uint8_t i = 0; i < blockyPool.size(); i++) {
        blockyPool[i]->updateFrame();
    }

    if (blockyIsSelectingInsertSlot) {
        for (auto btn : blockyInsertSlotBtns) {
            blockyInsertSlotBtn* slot = static_cast<blockyInsertSlotBtn*>(btn);
            slot->x = blockyStartX + getBlockyLength(slot->insertIndex) - 10;
            slot->y = 130;
        }

        for (auto btn : blockyNestedInsertSlotBtns) {
            blockyNestedInsertSlotBtn* slot = static_cast<blockyNestedInsertSlotBtn*>(btn);
            if (slot->owner == nullptr) {
                continue;
            }
            if (slot->toElse) {
                int16_t prefix = getNestedLengthPrefix(slot->owner->elseBlockPool, slot->insertIndex);
                int16_t elseBaseX = ifElseElseBaseX(slot->owner);
                slot->x = elseBaseX + prefix - 8;
                slot->y = slot->owner->y + 26;
            } else {
                int16_t prefix = getNestedLengthPrefix(slot->owner->ifBlockPool, slot->insertIndex);
                slot->x = ifElseIfBaseX(slot->owner) + prefix - 8;
                slot->y = slot->owner->y + 26;
            }
        }
    }

    if (selectedMainBlock != nullptr && !isMainBlockAlive(selectedMainBlock)) {
        clearSelection();
    }
    if (selectedNestedBlock != nullptr && !isNestedSelectionAlive()) {
        clearSelection();
    }

    if (!blockyIsSelectingInsertSlot && (selectedMainBlock != nullptr || selectedNestedBlock != nullptr)) {
        showDeleteButton();
    } else {
        clearDeleteButton();
    }
}

static void removeSelectedBlock() {
    if (selectedNestedBlock != nullptr && selectedNestedOwner != nullptr) {
        if (!isNestedSelectionAlive()) {
            clearSelection();
            clearDeleteButton();
            return;
        }
        std::vector<nestedBlockBase*>& targetPool = selectedNestedToElse ? selectedNestedOwner->elseBlockPool : selectedNestedOwner->ifBlockPool;
        for (auto it = targetPool.begin(); it != targetPool.end(); ++it) {
            if (*it == selectedNestedBlock) {
                delete *it;
                targetPool.erase(it);
                break;
            }
        }
        selectedNestedOwner->refreshWidthFromNested();
    } else if (selectedMainBlock != nullptr) {
        if (!isMainBlockAlive(selectedMainBlock)) {
            clearSelection();
            clearDeleteButton();
            return;
        }
        if (selectedMainBlock->isStart()) {
            // Start 节点保留，不能删。
        } else {
            for (auto it = blockyPool.begin(); it != blockyPool.end(); ++it) {
                if (*it == selectedMainBlock) {
                    delete *it;
                    blockyPool.erase(it);
                    reindexBlockyPool();
                    ensureStartAtFront();
                    break;
                }
            }
        }
    }

    clearSelection();
    clearDeleteButton();
}

static void blockyConfirmNestedInsert(blockyIfElse* owner, bool toElse, uint8_t insertIndex) {
    if (owner == nullptr) {
        return;
    }

    nestedBlockBase* newNested = nullptr;
    if (pendingCreateType == BLOCKY_CREATE_DELAY) {
        double delayValueX100 = uiInputNumberSliderX100();
        nestedDelayBlock* delayBlock = new nestedDelayBlock(delayValueX100);
        uint32_t bg = toElse ? TFT_DARKCYAN : TFT_GREEN;
        delayBlock->btn1 = new blockyNestedSelectBtn(owner, delayBlock, toElse,
            delayBlock->textBuffer, 0, 10, delayBlock->width, blockyHeight - 10, bg, TFT_BLACK);
        newNested = delayBlock;
    } else if (pendingCreateType == BLOCKY_CREATE_DIGITAL_OUT) {
        nestedDigitalWriteBlock* writeBlock = new nestedDigitalWriteBlock(blockyPendingIo, blockyPendingDigitalHigh);
        uint32_t bg = blockyPendingDigitalHigh ? TFT_GREEN : TFT_ORANGE;
        writeBlock->btn1 = new blockyNestedSelectBtn(owner, writeBlock, toElse,
            writeBlock->textBuffer, 0, 10, writeBlock->width, blockyHeight - 10, bg, TFT_BLACK);
        newNested = writeBlock;
    } else if (pendingCreateType == BLOCKY_CREATE_ANALOG_OUT) {
        nestedAnalogWriteBlock* writeBlock = new nestedAnalogWriteBlock(blockyPendingIo, blockyPendingAnalogDutyX100);
        writeBlock->btn1 = new blockyNestedSelectBtn(owner, writeBlock, toElse,
            writeBlock->textBuffer, 0, 10, writeBlock->width, blockyHeight - 10, TFT_MAGENTA, TFT_WHITE);
        newNested = writeBlock;
    } else {
        static char msg[48];
        snprintf(msg, sizeof(msg), "Nested only supports Delay now");
        popUp(msg);
    }

    if (newNested != nullptr) {
        std::vector<nestedBlockBase*>& targetPool = toElse ? owner->elseBlockPool : owner->ifBlockPool;
        if (insertIndex > targetPool.size()) {
            insertIndex = targetPool.size();
        }
        targetPool.insert(targetPool.begin() + insertIndex, newNested);
        owner->refreshWidthFromNested();
    }
}

static void runMainProgram() {
    ensureStartAtFront();
    for (uint8_t i = 1; i < blockyPool.size(); i++) {
        blockyPool[i]->execute();
    }
}

void blockyRuntimeStep() {
    if (blockyRunRequested) {
        blockyRunRequested = false;
        runMainProgram();
        renderActivityPtr = getActivity("blockyProgMain");
        uiRender();
        return;
    }

    if (blockyDeleteRequested) {
        blockyDeleteRequested = false;
        removeSelectedBlock();
        clearInsertSlotButtons();
        pendingCreateType = BLOCKY_CREATE_NONE;
        renderActivityPtr = getActivity("blockyProgMain");
        uiRender();
        return;
    }

    if (blockyPendingInsertIndex >= 0) {
        uint8_t insertIndex = (uint8_t)blockyPendingInsertIndex;
        blockyPendingInsertIndex = -1;
        blockyConfirmInsert(insertIndex);
        return;
    }

    if (blockyPendingNestedOwner != nullptr && blockyPendingNestedInsertIndex >= 0) {
        blockyIfElse* owner = blockyPendingNestedOwner;
        bool toElse = blockyPendingNestedToElse;
        uint8_t insertIndex = (uint8_t)blockyPendingNestedInsertIndex;
        blockyPendingNestedOwner = nullptr;
        blockyPendingNestedInsertIndex = -1;
        blockyConfirmNestedInsert(owner, toElse, insertIndex);
        clearInsertSlotButtons();
        pendingCreateType = BLOCKY_CREATE_NONE;
        renderActivityPtr = getActivity("blockyProgMain");
        uiRender();
    }
}

void blockyMoveLeft() {
    if (getBlockyLength() <= 320) return;
#if blockyMoveMode == 0
    int dx = -20;
    if (blockyStartX + dx + getBlockyLength() > 320) {
        blockyStartX += dx;
    } else {
        blockyStartX = -(getBlockyLength() - 320);
    }
#elif blockyMoveMode == 1
    int dx = 20;
    if (blockyStartX + dx < 0) {
        blockyStartX += dx;
    } else {
        blockyStartX = 0;
    }
#endif
    blockyControlSlider->percentage = (-blockyStartX) / (float)(getBlockyLength() - 320);
}

void blockyMoveRight() {
    if (getBlockyLength() <= 320) return;
#if blockyMoveMode == 0
    int dx = 20;
    if (blockyStartX + dx < 0) {
        blockyStartX += dx;
    } else {
        blockyStartX = 0;
    }
#elif blockyMoveMode == 1
    int dx = -20;
    if (blockyStartX + dx + getBlockyLength() > 320) {
        blockyStartX += dx;
    } else {
        blockyStartX = -(getBlockyLength() - 320);
    }
#endif
    blockyControlSlider->percentage = (-blockyStartX) / (float)(getBlockyLength() - 320);
}

static void selectDelayBlockType() {
    blockyStartSelectAddType(BLOCKY_CREATE_DELAY);
}

static bool ioPickerDone = false;
static bool ioPickerCanceled = false;
static ioDefine ioPickerValue = IO1;
static ioDefine blockyPickIoPin();

static bool levelPickerDone = false;
static bool levelPickerCanceled = false;
static bool levelPickerHigh = false;

static bool ifCondPickerDigitalHigh = true;
static bool ifCondPickerCanceled = false;

static ioDefine blockyPickIoPinForCondition() {
    return blockyPickIoPin();
}

static void selectIfDigitalHighFlow() {
    ioDefine io = blockyPickIoPinForCondition();
    if (ioPickerCanceled) {
        renderActivityPtr = getActivity("blockyProgAddFlow");
        uiRender();
        return;
    }
    blockyPendingIfIo = io;
    ifCondPickerDigitalHigh = true;
    pendingCreateType = BLOCKY_CREATE_IF_IO_HIGH;
    blockyStartSelectAddType(BLOCKY_CREATE_IF_IO_HIGH);
}

static void selectIfDigitalLowFlow() {
    ioDefine io = blockyPickIoPinForCondition();
    if (ioPickerCanceled) {
        renderActivityPtr = getActivity("blockyProgAddFlow");
        uiRender();
        return;
    }
    blockyPendingIfIo = io;
    ifCondPickerDigitalHigh = false;
    pendingCreateType = BLOCKY_CREATE_IF_IO_LOW;
    blockyStartSelectAddType(BLOCKY_CREATE_IF_IO_LOW);
}

static void selectIfAnalogGtFlow() {
    ioDefine io = blockyPickIoPinForCondition();
    if (ioPickerCanceled) {
        renderActivityPtr = getActivity("blockyProgAddFlow");
        uiRender();
        return;
    }
    double thresholdPct = uiInputNumberSliderX100();
    blockyPendingIfIo = io;
    blockyPendingIfCondValue = (int)(thresholdPct * 40.95f);
    blockyStartSelectAddType(BLOCKY_CREATE_IF_ANALOG_GT);
}

static void selectIfAnalogLtFlow() {
    ioDefine io = blockyPickIoPinForCondition();
    if (ioPickerCanceled) {
        renderActivityPtr = getActivity("blockyProgAddFlow");
        uiRender();
        return;
    }
    double thresholdPct = uiInputNumberSliderX100();
    blockyPendingIfIo = io;
    blockyPendingIfCondValue = (int)(thresholdPct * 40.95f);
    blockyStartSelectAddType(BLOCKY_CREATE_IF_ANALOG_LT);
}

static ioDefine blockyPickIoPin() {
    ioPickerDone = false;
    ioPickerCanceled = false;
    ioPickerValue = IO1;

    uiActivity* tempControlPtr = controlActivityPtr;
    uiActivity* tempRenderPtr = renderActivityPtr;

    if (getActivity("BlockyIOSelect") != nullptr) {
        deleteActivity("BlockyIOSelect");
    }
    controlActivityPtr = createActivity("BlockyIOSelect");
    renderActivityPtr = controlActivityPtr;

    new uiText("Select IO pin", 10, 10, 2);
    new uiButton("IO1", 10, 50, [](){ ioPickerValue = IO1; ioPickerDone = true; }, 70, 30, TFT_CYAN, TFT_BLACK);
    new uiButton("IO2", 90, 50, [](){ ioPickerValue = IO2; ioPickerDone = true; }, 70, 30, TFT_CYAN, TFT_BLACK);
    new uiButton("IO19", 170, 50, [](){ ioPickerValue = IO19; ioPickerDone = true; }, 70, 30, TFT_CYAN, TFT_BLACK);
    new uiButton("IO20", 250, 50, [](){ ioPickerValue = IO20; ioPickerDone = true; }, 70, 30, TFT_CYAN, TFT_BLACK);
    new uiButton("IO47", 10, 90, [](){ ioPickerValue = IO47; ioPickerDone = true; }, 70, 30, TFT_CYAN, TFT_BLACK);
    new uiButton("IO48", 90, 90, [](){ ioPickerValue = IO48; ioPickerDone = true; }, 70, 30, TFT_CYAN, TFT_BLACK);
    new uiButton("Back", 180, 90, [](){ ioPickerCanceled = true; ioPickerDone = true; }, 100, 30, TFT_LIGHTGREY, TFT_BLACK);

    uiRender();
    while (!ioPickerDone) {
        getTouch();
        btnMgr();
        vTaskDelay(5);
    }

    controlActivityPtr = tempControlPtr;
    renderActivityPtr = tempRenderPtr;
    deleteActivity("BlockyIOSelect");
    return ioPickerValue;
}

static bool blockyPickDigitalLevel(bool& canceled) {
    levelPickerDone = false;
    levelPickerCanceled = false;
    levelPickerHigh = false;

    uiActivity* tempControlPtr = controlActivityPtr;
    uiActivity* tempRenderPtr = renderActivityPtr;

    if (getActivity("BlockyLevelSelect") != nullptr) {
        deleteActivity("BlockyLevelSelect");
    }
    controlActivityPtr = createActivity("BlockyLevelSelect");
    renderActivityPtr = controlActivityPtr;

    new uiText("Digital output level", 10, 10, 2);
    new uiButton("HIGH", 50, 60, [](){ levelPickerHigh = true; levelPickerDone = true; }, 90, 35, TFT_GREEN, TFT_BLACK);
    new uiButton("LOW", 180, 60, [](){ levelPickerHigh = false; levelPickerDone = true; }, 90, 35, TFT_ORANGE, TFT_BLACK);
    new uiButton("Back", 110, 120, [](){ levelPickerCanceled = true; levelPickerDone = true; }, 100, 30, TFT_LIGHTGREY, TFT_BLACK);

    uiRender();
    while (!levelPickerDone) {
        getTouch();
        btnMgr();
        vTaskDelay(5);
    }

    controlActivityPtr = tempControlPtr;
    renderActivityPtr = tempRenderPtr;
    deleteActivity("BlockyLevelSelect");

    canceled = levelPickerCanceled;
    return levelPickerHigh;
}

static void blockyAppendNewMainBlock(blockyBase* newBlock) {
    if (newBlock == nullptr) {
        return;
    }
    blockyPool.push_back(newBlock);
    reindexBlockyPool();
    ensureStartAtFront();
    renderActivityPtr = getActivity("blockyProgMain");
    uiRender();
}

static void selectDigitalOutFlow() {
    ioDefine io = blockyPickIoPin();
    if (ioPickerCanceled) {
        renderActivityPtr = getActivity("blockyProgAdd");
        uiRender();
        return;
    }

    bool canceled = false;
    bool high = blockyPickDigitalLevel(canceled);
    if (canceled) {
        renderActivityPtr = getActivity("blockyProgAdd");
        uiRender();
        return;
    }

    blockyPendingIo = io;
    blockyPendingDigitalHigh = high;
    blockyStartSelectAddType(BLOCKY_CREATE_DIGITAL_OUT);
}

static void selectAnalogOutFlow() {
    ioDefine io = blockyPickIoPin();
    if (ioPickerCanceled) {
        renderActivityPtr = getActivity("blockyProgAdd");
        uiRender();
        return;
    }

    double dutyX100 = uiInputNumberSliderX100();

    blockyPendingIo = io;
    blockyPendingAnalogDutyX100 = dutyX100;
    blockyStartSelectAddType(BLOCKY_CREATE_ANALOG_OUT);
}

static void switchToBlockyProgAddRoot() {
    renderActivityPtr = getActivity("blockyProgAdd");
}

static void switchToBlockyProgAddOutput() {
    renderActivityPtr = getActivity("blockyProgAddOutput");
}

static void switchToBlockyProgAddFlow() {
    renderActivityPtr = getActivity("blockyProgAddFlow");
}

static void selectIfNumEq1Type() {
    selectIfDigitalHighFlow();
}

static void selectIfNumEq0Type() {
    selectIfDigitalLowFlow();
}

static void selectIfAnalogGtType() {
    selectIfAnalogGtFlow();
}

static void selectIfAnalogLtType() {
    selectIfAnalogLtFlow();
}

static void selectIo1HighType() {
    blockyStartSelectAddType(BLOCKY_CREATE_IF_IO_HIGH);
}

static void selectIo1LowType() {
    blockyStartSelectAddType(BLOCKY_CREATE_IF_IO_LOW);
}

static void selectIo1ReadType() {
    blockyStartSelectAddType(BLOCKY_CREATE_IO_READ);
}

static void selectInputNumType() {
    blockyStartSelectAddType(BLOCKY_CREATE_INPUT_NUM);
}

void blockyProgInit() {
    initBCP();
    pairDevice(targetMac);
    initIoMsg();
    uiActivity* tempControlPtr = controlActivityPtr;

    controlActivityPtr = createActivity("blockyProgMain");
    blockyControlSlider = new uiSlider("Control", 25, 220, 270, 0.0);
    new uiButton("<", 10, 170, blockyMoveLeft);
    new uiButton(">", 260, 170, blockyMoveRight);
    new uiButton("Add", 70, 170, switchToBlockyProgAdd);
    new uiDrawCallback(blockyMgr);
    blockyPool.push_back(new blockyStart((uint8_t)blockyPool.size()));
    ensureStartAtFront();

    // Add 根页面
    controlActivityPtr = createActivity("blockyProgAdd");
    new uiText("Choose block category", 10, 20, 2);
    new uiButton("Output", 50, 70, switchToBlockyProgAddOutput, 90, 36, TFT_GREEN, TFT_BLACK);
    new uiButton("Flow", 180, 70, switchToBlockyProgAddFlow, 90, 36, TFT_BLUE, TFT_WHITE);
    new uiButton("Back", 110, 180, switchToBlockyProgMain, 100, 30, TFT_LIGHTGREY, TFT_BLACK);

    // Add 输出类页面
    controlActivityPtr = createActivity("blockyProgAddOutput");
    new uiText("Output blocks", 10, 20, 2);
    new uiButton("DigitalOut", 35, 70, selectDigitalOutFlow, 110, 36, TFT_GREEN, TFT_BLACK);
    new uiButton("AnalogOut", 175, 70, selectAnalogOutFlow, 110, 36, TFT_MAGENTA, TFT_WHITE);
    new uiButton("Back", 110, 180, switchToBlockyProgAddRoot, 100, 30, TFT_LIGHTGREY, TFT_BLACK);

    // Add 流程控制类页面
    controlActivityPtr = createActivity("blockyProgAddFlow");
    new uiText("Flow blocks", 10, 8, 2);
    new uiButton("Delay", 10, 38, selectDelayBlockType, 95, 30, TFT_CYAN, TFT_BLACK);
    new uiButton("IF IO=H", 112, 38, selectIfNumEq1Type, 95, 30, TFT_BLUE, TFT_WHITE);
    new uiButton("IF IO=L", 214, 38, selectIfNumEq0Type, 95, 30, TFT_BLUE, TFT_WHITE);
    new uiButton("IF IO>", 10, 74, selectIfAnalogGtType, 95, 30, TFT_DARKCYAN, TFT_WHITE);
    new uiButton("IF IO<", 112, 74, selectIfAnalogLtType, 95, 30, TFT_DARKCYAN, TFT_WHITE);
    new uiButton("Back", 110, 180, switchToBlockyProgAddRoot, 100, 30, TFT_LIGHTGREY, TFT_BLACK);
    controlActivityPtr = tempControlPtr;
}

void blockyStartSelectAddType(blockyCreateType type) {
    pendingCreateType = type;
    blockyShowInsertSlots();
}

void blockyShowInsertSlots() {
    if (pendingCreateType == BLOCKY_CREATE_NONE) {
        return;
    }

    uiActivity* tempControlPtr = controlActivityPtr;
    controlActivityPtr = getActivity("blockyProgMain");

    clearInsertSlotButtons();
    blockyIsSelectingInsertSlot = true;

    for (uint8_t i = 1; i <= blockyPool.size(); i++) {
        int16_t plusX = blockyStartX + getBlockyLength(i) - 10;
        blockyInsertSlotBtns.push_back(new blockyInsertSlotBtn(i, plusX, 130));
    }

    for (uint8_t i = 0; i < blockyPool.size(); i++) {
        if (!blockyPool[i]->isIfElse()) {
            continue;
        }
        blockyIfElse* ifElse = static_cast<blockyIfElse*>(blockyPool[i]);

        for (uint8_t j = 0; j <= ifElse->ifBlockPool.size(); j++) {
            int16_t x = ifElseIfBaseX(ifElse) + getNestedLengthPrefix(ifElse->ifBlockPool, j) - 8;
            int16_t y = ifElse->y + 26;
            blockyNestedInsertSlotBtns.push_back(new blockyNestedInsertSlotBtn(ifElse, false, j, x, y));
        }

        for (uint8_t j = 0; j <= ifElse->elseBlockPool.size(); j++) {
            int16_t elseBaseX = ifElseElseBaseX(ifElse);
            int16_t x = elseBaseX + getNestedLengthPrefix(ifElse->elseBlockPool, j) - 8;
            int16_t y = ifElse->y + 26;
            blockyNestedInsertSlotBtns.push_back(new blockyNestedInsertSlotBtn(ifElse, true, j, x, y));
        }
    }

    controlActivityPtr = tempControlPtr;
    renderActivityPtr = getActivity("blockyProgMain");
    uiRender();
}

void blockyConfirmInsert(uint8_t insertIndex) {
    if (insertIndex == 0) {
        return;
    }

    blockyBase* newBlock = nullptr;
    if (pendingCreateType == BLOCKY_CREATE_DELAY) {
        double delayValueX100 = uiInputNumberSliderX100();
        newBlock = new blockyDelay(delayValueX100, insertIndex);
    } else if (pendingCreateType == BLOCKY_CREATE_DIGITAL_OUT) {
        newBlock = new blockyDigitalWrite(blockyPendingIo, blockyPendingDigitalHigh, insertIndex);
    } else if (pendingCreateType == BLOCKY_CREATE_ANALOG_OUT) {
        newBlock = new blockyAnalogWrite(blockyPendingIo, blockyPendingAnalogDutyX100, insertIndex);
    } else if (pendingCreateType == BLOCKY_CREATE_IO_READ) {
        newBlock = new blockyDigitalRead(insertIndex);
    } else if (pendingCreateType == BLOCKY_CREATE_INPUT_NUM) {
        newBlock = new blockyInputNum(insertIndex);
    } else if (pendingCreateType == BLOCKY_CREATE_IF_IO_HIGH) {
        newBlock = new blockyIfElse(insertIndex, BLOCKY_IF_IO_HIGH, blockyPendingIfIo);
    } else if (pendingCreateType == BLOCKY_CREATE_IF_IO_LOW) {
        newBlock = new blockyIfElse(insertIndex, BLOCKY_IF_IO_LOW, blockyPendingIfIo);
    } else if (pendingCreateType == BLOCKY_CREATE_IF_ANALOG_GT) {
        newBlock = new blockyIfElse(insertIndex, BLOCKY_IF_ANALOG_GT, blockyPendingIfIo, blockyPendingIfCondValue);
    } else if (pendingCreateType == BLOCKY_CREATE_IF_ANALOG_LT) {
        newBlock = new blockyIfElse(insertIndex, BLOCKY_IF_ANALOG_LT, blockyPendingIfIo, blockyPendingIfCondValue);
    }

    if (newBlock != nullptr) {
        if (insertIndex > blockyPool.size()) {
            insertIndex = blockyPool.size();
        }
        blockyPool.insert(blockyPool.begin() + insertIndex, newBlock);
        reindexBlockyPool();
        ensureStartAtFront();
    }

    clearInsertSlotButtons();
    pendingCreateType = BLOCKY_CREATE_NONE;
    renderActivityPtr = getActivity("blockyProgMain");
    uiRender();
}

void switchToBlockyProgAdd() {
    renderActivityPtr = getActivity("blockyProgAdd");
    BCPpinMode(targetMac, IO1, digitalWriteMode);
    BCPdigitalWrite(targetMac, IO1, 1);
}

void switchToBlockyProgMain() {
    BCPpinMode(targetMac, IO1, digitalWriteMode);
    BCPdigitalWrite(targetMac, IO1, 0);
    clearInsertSlotButtons();
    clearDeleteButton();
    pendingCreateType = BLOCKY_CREATE_NONE;
    blockyDeleteRequested = false;
    blockyPendingInsertIndex = -1;
    blockyPendingNestedOwner = nullptr;
    blockyPendingNestedInsertIndex = -1;
    selectedMainBlock = nullptr;
    selectedNestedOwner = nullptr;
    selectedNestedBlock = nullptr;
    renderActivityPtr = getActivity("blockyProgMain");
}

int16_t getBlockyLength(uint8_t index) {
    int16_t length = 0;
    for (uint8_t i = 0; i < index && i < blockyPool.size(); i++) {
        length += blockyPool[i]->width;
    }
    return length;
}
