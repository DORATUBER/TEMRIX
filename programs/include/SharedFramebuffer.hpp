#pragma once
#include <temrixstd.h>

#define MAX_SHARED_FB 16

static constexpr uint32_t MAX_INPUT_EVENTS = 16; 

enum SharedInputEventType : uint8_t
{
    InputEventMouse = 0,
    InputEventKeyboard = 1,
    InputEventFocusGained = 2,
    InputEventFocusLost = 3,
};

enum SharedWindowLayer : uint8_t
{
    LayerBackground = 0,
    LayerNormal = 1,
};

struct SharedInputEvent
{
    uint8_t type;
    uint32_t modifierKeys;
    uint32_t keyCodes[6];
    int32_t localX, localY; 
    int32_t deltaX, deltaY; 
    uint32_t buttons;
};

struct SharedFrameBufferSlot
{
    volatile uint32_t claimed;
    uint32_t shmem_id[2];
    uint32_t width;
    uint32_t height;
    uint32_t pixelsPerScanLine;
    int32_t x, y;
    int32_t dirtyX, dirtyY;
    uint32_t dirtyW, dirtyH;
    volatile uint32_t presentIndex;
    volatile uint32_t dirtyFrame;
    volatile uint32_t consumedFrame;
    bool ready;
    uint8_t focused;
    uint8_t layer;
    uint32_t inputWriteIndex;
    uint32_t inputReadIndex;
    SharedInputEvent inputEvents[MAX_INPUT_EVENTS];
    uint8_t _pad[2];
};

struct SharedFBRegistry
{
    SharedFrameBufferSlot slots[MAX_SHARED_FB];
};