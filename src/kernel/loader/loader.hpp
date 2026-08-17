#pragma once
#include "common.hpp"
#include "task.hpp"

struct KernelSection
{
    const uint8_t *src;
    uint64_t dst;
    uint64_t fileSize;
    uint64_t memSize;
    uint64_t vmFlags;
};

struct KernelReloc
{
    uint64_t offset;  
    int64_t  addend;
};

struct KernelSpawnInfo
{
    uint64_t entry;
    uint64_t stackSize;
    uint64_t bias;             
    KernelSection sections[8];
    uint32_t numSections;
    const KernelReloc *relocs; 
    uint32_t numRelocs;
};

namespace Loader
{
    bool spawn(const KernelSpawnInfo *info, Task **outTask);
}