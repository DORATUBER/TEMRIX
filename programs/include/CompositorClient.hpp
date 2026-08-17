#pragma once
#include "SharedFramebuffer.hpp"

static inline uint64_t CompositorParseU64(const char *s)
{
    uint64_t val = 0;
    while (*s >= '0' && *s <= '9')
    {
        val = val * 10 + (uint64_t)(*s - '0');
        s++;
    }
    return val;
}

class CompositorClient
{
public:
    bool init(int32_t x, int32_t y, uint32_t width, uint32_t height,
              uint8_t layer = LayerNormal)
    {
        char handleBuf[256];

        uint64_t got = 0;
        while ((got = Syscall::Service::Lookup("compositor", handleBuf, sizeof(handleBuf) - 1)) == 0)
            Syscall::Process::Yield();

        if (got != sizeof(uint64_t))
            return false;

        uint64_t handle = *reinterpret_cast<uint64_t *>(handleBuf);

        char pidBuf[256];
        while ((got = Syscall::Service::Lookup("compositor.pid", pidBuf, sizeof(pidBuf) - 1)) == 0)
            Syscall::Process::Yield();
        pidBuf[got] = '\0';
        m_compositorPid = (uint32_t)CompositorParseU64(pidBuf);

        Syscall::Memory::SharedMemResult reg;
        if (Syscall::Memory::MapShared(
                handle,
                ::Memory::Read | ::Memory::Write | ::Memory::User,
                &reg) != 0)
        {
            return false;
        }

        m_registry = reinterpret_cast<SharedFBRegistry *>(reg.virtAddr);

        if (!claimSlot())
            return false;

        if (!createPixelBuffers(width, height))
        {
            releaseSlot();
            return false;
        }

        SharedFrameBufferSlot &slot = m_registry->slots[m_slotIndex];

        slot.shmem_id[0] = (uint32_t)m_bufHandle[0];
        slot.shmem_id[1] = (uint32_t)m_bufHandle[1];
        slot.width = width;
        slot.height = height;
        slot.pixelsPerScanLine = width;
        slot.x = x;
        slot.y = y;
        slot.dirtyX = 0;
        slot.dirtyY = 0;
        slot.dirtyW = 0;
        slot.dirtyH = 0;
        slot.layer = layer;

        __atomic_store_n(&slot.presentIndex, 1, __ATOMIC_RELAXED);
        __atomic_store_n(&slot.dirtyFrame, 0, __ATOMIC_RELAXED);
        __atomic_store_n(&slot.consumedFrame, 0, __ATOMIC_RELAXED);
        __atomic_store_n(&slot.inputWriteIndex, 0, __ATOMIC_RELAXED);
        __atomic_store_n(&slot.inputReadIndex, 0, __ATOMIC_RELAXED);
        slot.focused = 0;

        m_drawIndex = 0;
        m_localFrame = 0;

        slot.ready = true;

        Syscall::Process::Notify(m_compositorPid);

        return true;
    }

    uint32_t *pixels() { return m_pixels[m_drawIndex]; }
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

    void present(int32_t dirtyX, int32_t dirtyY, uint32_t dirtyW, uint32_t dirtyH)
    {
        if (!m_registry)
            return;

        SharedFrameBufferSlot &slot = m_registry->slots[m_slotIndex];

        slot.dirtyX = dirtyX;
        slot.dirtyY = dirtyY;
        slot.dirtyW = dirtyW;
        slot.dirtyH = dirtyH;

        __atomic_store_n(&slot.presentIndex, (uint32_t)m_drawIndex, __ATOMIC_RELEASE);

        m_localFrame++;
        __atomic_store_n(&slot.dirtyFrame, m_localFrame, __ATOMIC_RELEASE);

        Syscall::Process::Notify(m_compositorPid);
    }

    SharedFrameBufferSlot *Slot()
    {
        if (!m_registry)
            return nullptr;
        return &m_registry->slots[m_slotIndex];
    }

    void presentAll()
    {
        present(0, 0, m_width, m_height);
    }

    void beginFrame()
    {
        if (!m_registry)
            return;

        m_drawIndex = 1 - m_drawIndex;

        if (m_localFrame < 2)
            return;

        SharedFrameBufferSlot &slot = m_registry->slots[m_slotIndex];
        uint32_t needConsumed = m_localFrame - 1;

        while (__atomic_load_n(&slot.consumedFrame, __ATOMIC_ACQUIRE) < needConsumed)
            Syscall::Process::Yield();
    }

    void move(int32_t x, int32_t y)
    {
        if (!m_registry)
            return;
        SharedFrameBufferSlot &slot = m_registry->slots[m_slotIndex];
        slot.x = x;
        slot.y = y;
    }

    void shutdown()
    {
        if (!m_registry)
            return;

        SharedFrameBufferSlot &slot = m_registry->slots[m_slotIndex];
        slot.ready = false;

        Syscall::Process::Notify(m_compositorPid);

        for (int b = 0; b < 2; b++)
        {
            if (m_pixels[b])
                Syscall::Memory::UnmapShared(reinterpret_cast<uint64_t>(m_pixels[b]), m_bufPages[b]);
        }

        releaseSlot();
        m_registry = nullptr;
    }

private:
    bool claimSlot()
    {
        for (int i = 0; i < MAX_SHARED_FB; i++)
        {
            uint32_t expected = 0;
            if (__atomic_compare_exchange_n(
                    &m_registry->slots[i].claimed,
                    &expected, 1,
                    false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
            {
                m_slotIndex = i;
                return true;
            }
        }
        return false;
    }

    void releaseSlot()
    {
        __atomic_store_n(&m_registry->slots[m_slotIndex].claimed, 0, __ATOMIC_RELEASE);
    }

    bool createPixelBuffers(uint32_t width, uint32_t height)
    {
        uint64_t size = (uint64_t)width * height * sizeof(uint32_t);

        for (int b = 0; b < 2; b++)
        {
            m_bufPages[b] = (size + 0xFFF) >> 12;
            if (m_bufPages[b] == 0)
                m_bufPages[b] = 1;

            Syscall::Memory::SharedMemResult buf;
            if (Syscall::Memory::CreateShared(
                    m_bufPages[b],
                    ::Memory::Read | ::Memory::Write | ::Memory::User,
                    &buf) != 0)
            {
                if (b == 1 && m_pixels[0])
                    Syscall::Memory::UnmapShared(reinterpret_cast<uint64_t>(m_pixels[0]), m_bufPages[0]);
                return false;
            }

            m_bufHandle[b] = buf.handle;
            m_pixels[b] = reinterpret_cast<uint32_t *>(buf.virtAddr);

            uint64_t *p64 = reinterpret_cast<uint64_t *>(m_pixels[b]);
            for (uint64_t i = 0; i < (m_bufPages[b] * 0x1000) / 8; i++)
                p64[i] = 0;
        }

        m_width = width;
        m_height = height;
        return true;
    }

private:
    SharedFBRegistry *m_registry = nullptr;
    int m_slotIndex = -1;
    uint32_t m_compositorPid = 0;

    uint64_t m_bufHandle[2] = {0, 0};
    uint64_t m_bufPages[2] = {0, 0};
    uint32_t *m_pixels[2] = {nullptr, nullptr};

    uint32_t m_width = 0;
    uint32_t m_height = 0;

    int m_drawIndex = 0;
    uint32_t m_localFrame = 0;
};