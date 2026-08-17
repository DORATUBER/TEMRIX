#pragma once
#include "InputShared.hpp"

class InputServer
{
public:
    bool init()
    {
        uint64_t pages = (sizeof(InputRegistry) + 0xFFF) >> 12;

        Syscall::Memory::SharedMemResult reg;
        if (Syscall::Memory::CreateShared(
                pages,
                ::Memory::Read | ::Memory::Write | ::Memory::User,
                &reg) != 0)
        {
            return false;
        }

        m_registry = reinterpret_cast<InputRegistry *>(reg.virtAddr);

        uint8_t *raw = reinterpret_cast<uint8_t *>(m_registry);
        for (uint64_t i = 0; i < sizeof(InputRegistry); i++)
            raw[i] = 0;

        char handleStr[21];
        Syscall::Service::Publish("input.reg", String::FromU64(reg.handle, handleStr));

        uint32_t myPid = Syscall::Process::GetId();
        char pidStr[21];
        Syscall::Service::Publish("input.pid", String::FromU64(myPid, pidStr));

        __atomic_store_n(&m_registry->ready, 1, __ATOMIC_RELEASE);

        String::Print("[input-server] ready\n");

        return true;
    }

    void PushKeyboardEvent(uint8_t modifierKeys, const uint8_t keyCodes[6])
    {
        InputEvent &event = nextSlot();
        event.type = InputEventTypeKeyboard;
        event.modifierKeys = modifierKeys;
        for (int i = 0; i < 6; i++)
            event.keyCodes[i] = keyCodes[i];
        publish(event);
    }

    void PushMouseEvent(uint8_t buttons, int8_t deltaX, int8_t deltaY)
    {
        InputEvent &event = nextSlot();
        event.type = InputEventTypeMouse;
        event.mouseButtons = buttons;
        event.mouseDeltaX = deltaX;
        event.mouseDeltaY = deltaY;
        publish(event);
    }

private:
    InputEvent &nextSlot()
    {
        uint32_t writeIndex = __atomic_load_n(&m_registry->writeSequence, __ATOMIC_RELAXED);
        return m_registry->events[writeIndex % InputRingSize];
    }

    void publish(InputEvent &)
    {
        uint32_t writeIndex = __atomic_load_n(&m_registry->writeSequence, __ATOMIC_RELAXED);
        __atomic_store_n(&m_registry->writeSequence, writeIndex + 1, __ATOMIC_RELEASE);

        uint32_t focusedId = __atomic_load_n(&m_registry->focusedTaskId, __ATOMIC_ACQUIRE);
        if (focusedId != 0)
            Syscall::Process::Notify(focusedId);
    }

    InputRegistry *m_registry = nullptr;
};