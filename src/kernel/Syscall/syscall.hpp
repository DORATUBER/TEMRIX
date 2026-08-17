#pragma once
#include "common.hpp"

struct SyscallFrame
{
    uint64_t r15, r14, r13, r12, rbp, rbx, r11, r10, r9, r8, rdi, rsi, rdx, rcx, rax;
};

struct SyscallResult
{
    uint64_t rax;
    uint64_t rdx;
};

struct DmaAllocResult
{
    uint64_t virt;
    uint64_t phys;
    uint64_t size;
};

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

struct VectorAllocResult
{
    uint8_t base0;
    uint8_t count0;
    uint8_t base1;
    uint8_t count1;
    uint8_t rangeCount;
};

struct MsixInfo
{
    uint8_t  supported;
    uint16_t count;
};

enum InfoType : uint64_t
{
    InfoFramebuffer = 1,
    InfoCpu = 2,
    InfoMemory = 3,
    InfoPciDevice = 4,     
    InfoSharedRegion = 5,
};

struct SharedMemoryResult
{
    uint64_t handle;
    uint64_t virtAddr;
    uint64_t pageCount;
};

static constexpr uint64_t USER_SPACE_MAX = 0x0000800000000000ULL;

extern "C" SyscallResult syscallHandler(SyscallFrame *f);