#pragma once

#include <temrixstd/stdint.h>
#include <temrixstd/stdbool.h>
#include <temrixstd/temrix.h>

namespace Syscall
{
    namespace Service
    {
        static inline bool Publish(const char *name, uint64_t nameLen, const char *data, uint64_t dataLen)
        {
            return syscall4(SyscallPublish, (uint64_t)name, nameLen, (uint64_t)data, dataLen) != 0;
        }

        static inline bool Publish(const char *name, const char *data)
        {
            uint64_t nameLen = 0;
            while (name[nameLen])
                nameLen++;
            uint64_t dataLen = 0;
            while (data[dataLen])
                dataLen++;
            return syscall4(SyscallPublish, (uint64_t)name, nameLen, (uint64_t)data, dataLen) != 0;
        }

        static inline bool Publish(const char *name, const void *data, uint64_t dataLen)
        {
            uint64_t nameLen = 0;
            while (name[nameLen])
                nameLen++;
            return syscall4(SyscallPublish, (uint64_t)name, nameLen, (uint64_t)data, dataLen) != 0;
        }

        static inline uint64_t Lookup(const char *name, uint64_t nameLen, char *out, uint64_t outCapacity)
        {
            return syscall4(SyscallLookup, (uint64_t)name, nameLen, (uint64_t)out, outCapacity);
        }

        static inline uint64_t Lookup(const char *name, char *out, uint64_t outCapacity)
        {
            uint64_t nameLen = 0;
            while (name[nameLen])
                nameLen++;
            return syscall4(SyscallLookup, (uint64_t)name, nameLen, (uint64_t)out, outCapacity);
        }
    }
}
