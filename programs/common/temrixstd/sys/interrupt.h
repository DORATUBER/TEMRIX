#pragma once

#include <temrixstd/stdint.h>
#include <temrixstd/temrix.h>

namespace Syscall
{
    namespace Interrupt
    {
        struct VectorAllocResult
        {
            uint8_t base0;
            uint8_t count0;
            uint8_t base1;
            uint8_t count1;
            uint8_t rangeCount;
        };

        static inline uint64_t AllocVectors(uint64_t count, VectorAllocResult *out)
        {
            return syscall2(SyscallAllocVectors, count, (uint64_t)out);
        }

        static inline uint64_t Subscribe(uint8_t vector)
        {
            return syscall1(SyscallSubscribeIrq, (uint64_t)vector);
        }
    }
}
