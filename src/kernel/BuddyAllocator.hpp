#pragma once
#include "MemoryCommon.hpp"

namespace Memory
{
    class BuddyAllocator
    {
    public:
        void init(BootInfo* info);
        void *malloc(uint32_t size);
        void *mallocAligned(uint32_t size, uint32_t align);
        void free(void *ptr);
        MemoryRegion* getMemoryRegion() { return regions; }
        uint32_t getNumberOfRegions() { return num_regions; }
        uint64_t getFreeMemory();

    private:
        Spinlock m_lock;
        MemoryRegion regions[MAX_REGIONS];
        uint32_t num_regions = 0;
        uint64_t free_memory;

        void *mallocImplementation(uint32_t size);
        void freeImplementation(void *ptr);

        MemoryRegion* findRegionByPhys(uint64_t physAddr, uint32_t& pageOut);
        bool unlinkBlock(MemoryRegion* region, int order, uint32_t page);
        void pushFreeBlock(MemoryRegion* region, int order, uint32_t page);
        uint32_t getBuddyPage(uint32_t page, int order);
        bool isAligned(uint32_t page, int order);
    };
}