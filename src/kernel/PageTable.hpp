#pragma once
#include "MemoryCommon.hpp"
#include "BuddyAllocator.hpp"

namespace Memory
{
    struct PageTableContext
    {
        uint64_t *pml4;
        BuddyAllocator *alloc;
    };

    static constexpr uint64_t PAGE_PRESENT  = 1ULL << 0;
    static constexpr uint64_t PAGE_WRITABLE = 1ULL << 1;
    static constexpr uint64_t PAGE_USER     = 1ULL << 2;
    static constexpr uint64_t PAGE_PWT      = 1ULL << 3;
    static constexpr uint64_t PAGE_PCD      = 1ULL << 4;
    static constexpr uint64_t PAGE_HUGE     = 1ULL << 7;

    void initPageTables(PageTableContext *ctx, BootInfo *info);
    void freePageTable(PageTableContext* ctx, BuddyAllocator* alloc);
    void mapPage2MB(PageTableContext *ctx, uint64_t virt, uint64_t phys, uint64_t flags);
    void mapPage4KB(PageTableContext *ctx, uint64_t virt, uint64_t phys, uint64_t flags);
    void mapMMIO(PageTableContext *ctx, uint64_t phys, uint64_t size);

    uint64_t virtToPhys(PageTableContext *ctx, uint64_t virt);

    void syncKernelMappings(PageTableContext *proc_ctx, PageTableContext *kernel_ctx);
    PageTableContext createProcessPageTable(PageTableContext *kernel_ctx, BuddyAllocator *alloc);

    inline void loadPageTable(PageTableContext *ctx)
    {
        asm volatile("mov %0, %%cr3" ::"r"((uint64_t)ctx->pml4) : "memory");
    }
}