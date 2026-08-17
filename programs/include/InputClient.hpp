#pragma once
#include "InputShared.hpp"

static inline uint64_t InputToU64(const char *s)
{
    uint64_t val = 0;
    while (*s >= '0' && *s <= '9')
    {
        val = val * 10 + (uint64_t)(*s - '0');
        s++;
    }
    return val;
}

class InputClient
{
public:
    bool init()
    {
        char regBuf[256];
        uint64_t got = 0;
        while ((got = Syscall::Service::Lookup("input.reg", regBuf, sizeof(regBuf) - 1)) == 0)
            Syscall::Process::Yield();
        regBuf[got] = '\0';
        uint64_t handle = InputToU64(regBuf);

        Syscall::Memory::SharedMemResult reg;
        if (Syscall::Memory::MapShared(handle, ::Memory::Read | ::Memory::Write | ::Memory::User, &reg) != 0)
            return false;

        m_registry = reinterpret_cast<InputRegistry *>(reg.virtAddr);

        while (__atomic_load_n(&m_registry->ready, __ATOMIC_ACQUIRE) == 0)
            Syscall::Process::Yield();

        m_lastSequence = __atomic_load_n(&m_registry->writeSequence, __ATOMIC_ACQUIRE);
        return true;
    }

    void RequestFocus()
    {
        if (!m_registry)
            return;
        uint32_t myId = Syscall::Process::GetId();
        __atomic_store_n(&m_registry->focusedTaskId, myId, __ATOMIC_RELEASE);
    }

    template <typename Fn>
    void Poll(Fn onEvent)
    {
        uint32_t currentSequence = __atomic_load_n(&m_registry->writeSequence, __ATOMIC_ACQUIRE);
        if (currentSequence == m_lastSequence)
            return;

        uint32_t pending = currentSequence - m_lastSequence;
        if (pending > InputRingSize)
            pending = InputRingSize; 

        uint32_t startSequence = currentSequence - pending;
        for (uint32_t seq = startSequence; seq != currentSequence; seq++)
            onEvent(m_registry->events[seq % InputRingSize]);

        m_lastSequence = currentSequence;
    }

private:
    InputRegistry *m_registry = nullptr;
    uint32_t m_lastSequence = 0;
};