#pragma once

#include <temrixstd/stdint.h>
#include <temrixstd/stdbool.h>
#include <temrixstd/temrix.h>
#include "loader_types.h"

namespace Syscall
{
    namespace Process
    {
        static inline void Yield()
        {
            syscall0(SyscallYield);
        }

        static inline void Exit(uint64_t code = 0)
        {
            syscall1(SyscallExit, code);
        }

        static inline void Wait()
        {
            syscall0(SyscallWait);
        }

        static inline void Notify(uint32_t pid)
        {
            syscall1(SyscallNotify, pid);
        }

        static inline uint32_t GetId()
        {
            return (uint32_t)syscall0(SyscallGetPid);
        }

        enum Capability : uint64_t
        {
            CAP_NONE          = 0,
            CAP_WRITE         = 1ull << 0,
            CAP_PCI_MSIX      = 1ull << 1,
            CAP_IRQ           = 1ull << 2,
            CAP_ALLOC_VECTORS = 1ull << 3,
            CAP_GRANT         = 1ull << 4,

            CAP_ALL_SIMPLE = CAP_WRITE | CAP_PCI_MSIX | CAP_IRQ | CAP_ALLOC_VECTORS | CAP_GRANT,
        };

        enum class DeviceGrantKind : uint8_t
        {
            SpecificDevice = 0,
            ClassWildcard  = 1,
            AnyDevice      = 2,
        };

        struct MapDescriptor
        {
            uint64_t dst;
            uint64_t pageCount;
            uint64_t flags;
            bool zero;
        };

        struct CopyDescriptor
        {
            uint64_t src;
            uint64_t dst;
            uint64_t size;
        };

        enum class SpawnReg : uint8_t
        {
            RDI,
            RSI,
            RDX,
            RCX,
            R8,
            R9,
            RBX,
            RBP,
            R12,
            R13,
            R14,
            R15,
            Count
        };

        struct RegisterValue
        {
            uint8_t regIndex;
            uint64_t value;
        };

        struct SpawnInfo
        {
            MapDescriptor *maps;
            uint32_t numMaps;
            CopyDescriptor *copies;
            uint32_t numCopies;
            RegisterValue *registers;
            uint32_t numRegisters;
            uint64_t entry;
            uint64_t stackPointer;
            uint64_t requestedCapabilities = 0;

            uint8_t deviceGrantKind = 0;
            bool requestDeviceGrant = false;
            uint64_t deviceGrantParam = 0;
        };

        static inline uint32_t Spawn(SpawnInfo *info)
        {
            return (uint32_t)syscall1(SyscallSpawn, (uint64_t)info);
        }
    }
}

uint32_t spawnBinaryImage(const char *name,
                          const uint8_t *fileBytes,
                          uint64_t entryPoint,
                          const Section *sections, uint32_t numSections,
                          const Relocation *relocs, uint32_t numRelocs,
                          const LoaderConfig &cfg,
                          const Syscall::Process::CopyDescriptor *extraCopies, uint32_t numExtraCopies,
                          const Syscall::Process::RegisterValue *extraRegs, uint32_t numExtraRegs,
                          uint64_t requestedCapabilities,
                          bool requestDeviceGrant,
                          uint8_t deviceGrantKind,
                          uint64_t deviceGrantParam);

uint32_t spawnFromBuffer(const char *path, uint8_t *buf, uint32_t fileSize,
                          const Syscall::Process::CopyDescriptor *extraCopies = nullptr, uint32_t numExtraCopies = 0,
                          const Syscall::Process::RegisterValue *extraRegs = nullptr, uint32_t numExtraRegs = 0,
                          uint64_t requestedCapabilities = 0,
                          bool requestDeviceGrant = false,
                          uint8_t deviceGrantKind = 0,
                          uint64_t deviceGrantParam = 0);

uint32_t spawnFromBufferWithArgs(const char *path, uint8_t *buf, uint32_t fileSize,
                                  int argc, const char *const argv[],
                                  uint64_t requestedCapabilities,
                                  bool requestDeviceGrant,
                                  uint8_t deviceGrantKind,
                                  uint64_t deviceGrantParam);