#include "blockyProg.h"
#include "Arduino.h"
#include <cstdio>

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
static blockyBase* selectedMainBlock = nullptr;
static blockyIfElse* selectedNestedOwner = nullptr;
static nestedBlockBase* selectedNestedBlock = nullptr;
static bool selectedNestedToElse = false;
static uiButton* blockyDeleteBtn = nullptr;

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
    btn1 = new blockyMainSelectBtn(this, "Start", x, y, width, blockyHeight, TFT_CYAN, TFT_BLACK);
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

void nestedDelayBlock::updateFrame(int16_t startX, int16_t startY) {
    snprintf(textBuffer, sizeof(textBuffer), "Dly %.1f", delayValueX100 / 100.0f);
    btn1->text = textBuffer;
    btn1->x = startX;
    btn1->y = startY;
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

blockyIfElse::blockyIfElse(uint8_t index) {
    this->index = index;
    y = 10;
    ifTagBtn = new blockyMainSelectBtn(this, "IF", 0, y, 34, 24, TFT_BLUE, TFT_WHITE);
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

void blockyRuntimeStep() {
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

static void selectIfElseBlockType() {
    blockyStartSelectAddType(BLOCKY_CREATE_IFELSE);
}

void blockyProgInit() {
    uiActivity* tempControlPtr = controlActivityPtr;

    controlActivityPtr = createActivity("blockyProgMain");
    blockyControlSlider = new uiSlider("Control", 25, 220, 270, 0.0);
    new uiButton("<", 10, 170, blockyMoveLeft);
    new uiButton(">", 260, 170, blockyMoveRight);
    new uiButton("Add", 70, 170, switchToBlockyProgAdd);
    new uiDrawCallback(blockyMgr);
    blockyPool.push_back(new blockyStart((uint8_t)blockyPool.size()));
    ensureStartAtFront();

    controlActivityPtr = createActivity("blockyProgAdd");
    new uiText("Choose block type", 10, 20, 2);
    new uiButton("Delay", 20, 70, selectDelayBlockType, 120, 36, TFT_CYAN, TFT_BLACK);
    new uiButton("If Else", 170, 70, selectIfElseBlockType, 120, 36, TFT_BLUE, TFT_WHITE);
    new uiButton("Back", 110, 170, switchToBlockyProgMain, 100, 30, TFT_LIGHTGREY, TFT_BLACK);
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
    } else if (pendingCreateType == BLOCKY_CREATE_IFELSE) {
        newBlock = new blockyIfElse(insertIndex);
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
}

void switchToBlockyProgMain() {
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
