#pragma once
#include "kernel/common.hpp"
#include "kernel/MemoryCommon.hpp"
#include "kernel/BuddyAllocator.hpp"
#include "kernel/spinlock.hpp"

namespace Memory
{
    class SlabAllocator
    {
    public:
        void init(BuddyAllocator *buddy);

        void *malloc(uint32_t size);
        void *mallocAligned(uint32_t size, uint32_t align);
        void free(void *ptr);

    private:
        static constexpr uint32_t CLASS_SIZES[] = {32, 64, 128, 256, 512, 1024, 2048};
        static constexpr uint32_t NUM_CLASSES = 7;
        static constexpr uint32_t LARGEST_CLASS = 2048;
        static constexpr uint32_t SLAB_MAGIC = 0x5A4C4142; 

        struct FreeChunk
        {
            FreeChunk *next;
        };

        struct SlabPageHeader
        {
            uint32_t magic;
            uint32_t sizeClassIdx;
            uint32_t usedCount;
            FreeChunk *freeList;      
            SlabPageHeader *nextPage; 
        };
        static_assert(sizeof(SlabPageHeader) <= 32, "SlabPageHeader must fit in the smallest class");

        struct SlabClass
        {
            SlabPageHeader *pages = nullptr;
        };

        BuddyAllocator *m_buddy = nullptr;
        Spinlock m_lock;
        SlabClass m_classes[NUM_CLASSES];

        static int classFor(uint32_t size, uint32_t align);

        void *allocFromClassLocked(int classIdx);
        SlabPageHeader *createPageLocked(int classIdx);
    };
}