#pragma once

#include <temrixstd/stdint.h>

extern "C"
{
    uint64_t syscall0(uint64_t num);
    uint64_t syscall1(uint64_t num, uint64_t a1);
    uint64_t syscall2(uint64_t num, uint64_t a1, uint64_t a2);
    uint64_t syscall3(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3);
    uint64_t syscall4(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4);
    uint64_t syscall5(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);
    uint64_t syscall6(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
}

enum SyscallNumber : uint64_t
{
    SyscallGetMemoryMap = 0,
    SyscallMmap = 1,
    SyscallMunmap = 2,
    SyscallAllocDma = 3,
    SyscallCreateShared = 4,
    SyscallDestroyShared = 5,
    SyscallMapShared = 6,
    SyscallUnmapShared = 7,
    SyscallMapBar = 8,
    SyscallUnmapBar = 9,
    SyscallPciGetDevices = 10,
    SyscallPublish = 11,
    SyscallLookup = 12,
    SyscallYield = 13,
    SyscallExit = 14,
    SyscallWait = 15,
    SyscallNotify = 16,
    SyscallGetPid = 17,
    SyscallSpawn = 18,
    SyscallGetInfo = 19,
    SyscallSubscribeIrq = 20,
    SyscallAllocVectors = 21,
    SyscallPciMsixInfo   = 22, 
    SyscallPciMsixEnable = 23,
    SyscallGrantCapability = 24,
    SyscallRevokeCapability = 25,
    SyscallGrantDeviceAccess = 26,
    SyscallWrite = 255,
};