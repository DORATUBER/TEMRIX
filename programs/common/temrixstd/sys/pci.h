#pragma once

#include <temrixstd/stdint.h>
#include <temrixstd/stdbool.h>
#include <temrixstd/temrix.h>

namespace Syscall
{
    namespace Pci
    {
        struct KernelDevice
        {
            uint8_t bus, dev, func;
            uint16_t vendorId;
            uint16_t deviceId;
            uint8_t classCode;
            uint8_t subclass;
            uint8_t revision;
            uint64_t bars[6];
            uint64_t barSizes[6];
            bool valid;
        };

        static inline uint64_t Count()
        {
            return syscall2(SyscallPciGetDevices, 0, 0);
        }

        static inline uint64_t GetDevices(KernelDevice *buf, uint64_t count)
        {
            return syscall2(SyscallPciGetDevices, (uint64_t)buf, count);
        }

        struct MsixInfo
        {
            uint8_t supported;
            uint16_t count;
        };

        static inline uint64_t MsixInfoGet(uint64_t deviceIndex, MsixInfo *out)
        {
            return syscall2(SyscallPciMsixInfo, deviceIndex, (uint64_t)out);
        }

        static inline uint64_t MsixEnable(uint64_t deviceIndex, uint64_t entry, uint8_t vector)
        {
            return syscall3(SyscallPciMsixEnable, deviceIndex, entry, (uint64_t)vector);
        }
    }
}
