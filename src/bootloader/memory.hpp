#pragma once
#include "efi.hpp"

namespace Memory
{
    static constexpr uint64_t PAGE_PRESENT  = 1ULL << 0;
    static constexpr uint64_t PAGE_WRITABLE = 1ULL << 1;
    static constexpr uint64_t PAGE_PWT      = 1ULL << 3;
    static constexpr uint64_t PAGE_PCD      = 1ULL << 4;
    static constexpr uint64_t PAGE_HUGE     = 1ULL << 7;

    struct BumpAllocator {
        uint8_t* base;
        uint64_t offset;
        uint64_t size;

        void* alloc(uint64_t bytes) {
            void* ptr = base + offset;
            offset += (bytes + 0xFFFULL) & ~0xFFFULL;
            return ptr;
        }
    };

    struct PageTableContext {
        uint64_t*      pml4;
        BumpAllocator* allocator;
    };

    void initPageTables(PageTableContext* ctx,
                        uint64_t kernelPhys,
                        uint64_t kernelSize,
                        uint64_t fbBase,
                        uint32_t fbPps,
                        uint32_t fbHeight,
                        uint64_t stackPhys,
                        uint64_t stackVirt,
                        uint64_t stackSize,
                        uint8_t* mmap,
                        uint32_t mmapSize,
                        uint32_t descSize);
}
