#pragma once
#include <temrixstd.h>

static constexpr uint32_t InputRingSize = 64;

enum InputEventType : uint32_t
{
    InputEventTypeKeyboard = 1,
    InputEventTypeMouse = 2,
};

struct InputEvent
{
    uint32_t type;
    uint8_t modifierKeys;
    uint8_t keyCodes[6];
    uint8_t mouseButtons;
    int8_t mouseDeltaX;
    int8_t mouseDeltaY;
};

static constexpr uint32_t MaxInputConsumers = 8;

struct InputRegistry
{
    uint32_t ready;
    uint32_t focusedTaskId;
    uint32_t writeSequence;
    InputEvent events[InputRingSize];
};