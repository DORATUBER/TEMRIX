#pragma once

#include <temrixstd/stdint.h>
#include <temrixstd/temrix.h>

enum InfoType : uint64_t
{
    InfoFramebuffer = 1,
    InfoCpu = 2,
    InfoMemory = 3,
    InfoPciDevice = 4,     
    InfoSharedRegion = 5,
};

struct FramebufferInfo
{
    uint64_t physAddr;
    uint32_t width;
    uint32_t height;
    uint32_t pixelsPerScanLine;
};

static constexpr uint64_t KERNEL_RO_DATA_ADDRESS = 0x3FE000;
static constexpr uint64_t KERNEL_RW_DATA_ADDRESS = 0x3FF000;

struct KernelReadOnlyData
{
    volatile uint64_t ticks;
    volatile uint64_t ticksPerSecond;
    volatile uint8_t kbBuf[256];
    volatile uint8_t kbHead;
    uint8_t _pad[4096 - (8 + 8 + 256 + 1)];
};

struct KernelReadWriteData
{
    volatile uint8_t kbTail;
    uint8_t _pad[4096 - 1];
};

namespace Syscall
{
    namespace Info
    {
        static inline uint64_t Get(InfoType type, uint64_t param, void *out)
        {
            return syscall3(SyscallGetInfo, (uint64_t)type, param, (uint64_t)out);
        }
        static inline uint64_t Get(InfoType type, void *out)
        {
            return Get(type, 0, out);
        }
    }
}
