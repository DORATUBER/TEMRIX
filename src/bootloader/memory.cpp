#include "memory.hpp"

namespace Memory
{
    inline void loadPageTable(PageTableContext* ctx) {
        asm volatile("mov %0, %%cr3" :: "r"((uint64_t)ctx->pml4) : "memory");
    }

    static uint64_t* allocTable(PageTableContext* ctx) {
        uint64_t* table = (uint64_t*)ctx->allocator->alloc(4096);
        for (int i = 0; i < 512; i++) table[i] = 0;
        return table;
    }

    static uint64_t* getOrCreateTable(PageTableContext* ctx,
                                      uint64_t* parent,
                                      uint16_t index) {
        if (parent[index] & PAGE_PRESENT)
            return (uint64_t*)(parent[index] & ~0xFFFULL);

        uint64_t* table = allocTable(ctx);
        parent[index] = (uint64_t)table | PAGE_PRESENT | PAGE_WRITABLE;
        return table;
    }

    static void mapPage2MB(PageTableContext* ctx,
                           uint64_t virt,
                           uint64_t phys,
                           uint64_t flags) {
        virt &= ~0x1FFFFFULL;
        phys &= ~0x1FFFFFULL;

        uint16_t pml4Idx = (virt >> 39) & 0x1FF;
        uint16_t pdptIdx = (virt >> 30) & 0x1FF;
        uint16_t pdIdx   = (virt >> 21) & 0x1FF;

        uint64_t* pdpt = getOrCreateTable(ctx, ctx->pml4, pml4Idx);
        uint64_t* pd   = getOrCreateTable(ctx, pdpt, pdptIdx);

        pd[pdIdx] = phys | flags | PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE;
    }

    static void mapPage4KB(PageTableContext* ctx,
                           uint64_t virt,
                           uint64_t phys,
                           uint64_t flags) {
        virt &= ~0xFFFULL;
        phys &= ~0xFFFULL;

        uint16_t pml4Idx = (virt >> 39) & 0x1FF;
        uint16_t pdptIdx = (virt >> 30) & 0x1FF;
        uint16_t pdIdx   = (virt >> 21) & 0x1FF;
        uint16_t ptIdx   = (virt >> 12) & 0x1FF;

        uint64_t* pdpt = getOrCreateTable(ctx, ctx->pml4, pml4Idx);
        uint64_t* pd   = getOrCreateTable(ctx, pdpt, pdptIdx);
        uint64_t* pt   = getOrCreateTable(ctx, pd, pdIdx);

        pt[ptIdx] = phys | flags | PAGE_PRESENT | PAGE_WRITABLE;
    }

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
                        uint32_t descSize) {
        ctx->pml4 = allocTable(ctx);

        uint32_t numEntries = mmapSize / descSize;
        for (uint32_t i = 0; i < numEntries; i++) {
            uint8_t*  desc  = mmap + (i * descSize);
            uint64_t  addr  = *(uint64_t*)(desc + 8);
            uint64_t  pages = *(uint64_t*)(desc + 24);
            uint64_t  size  = pages * 0x1000;

            uint64_t start = addr & ~0x1FFFFFULL;
            uint64_t end   = (addr + size + 0x1FFFFFULL) & ~0x1FFFFFULL;

            for (uint64_t a = start; a < end; a += 0x200000)
                mapPage2MB(ctx, a, a, 0);

            for (uint64_t a = start; a < end; a += 0x200000)
                mapPage2MB(ctx, 0xffff888000000000ULL + a, a, 0);
        }

        uint64_t virtBase = 0xFFFFFFFF80000000ULL;
        uint64_t pages    = (kernelSize + 0xFFF) & ~0xFFFULL;
        for (uint64_t off = 0; off < pages; off += 0x1000)
            mapPage4KB(ctx, virtBase + off, kernelPhys + off, 0);

        for (uint64_t off = 0; off < stackSize; off += 0x1000)
            mapPage4KB(ctx, stackVirt + off, stackPhys + off, 0);

        uint64_t fbSize  = (uint64_t)fbPps * fbHeight * 4;
        uint64_t fbStart = fbBase & ~0x1FFFFFULL;
        uint64_t fbEnd   = (fbBase + fbSize + 0x1FFFFFULL) & ~0x1FFFFFULL;

        for (uint64_t a = fbStart; a < fbEnd; a += 0x200000)
            mapPage2MB(ctx, a, a, PAGE_PWT | PAGE_PCD);

        for (uint64_t a = fbStart; a < fbEnd; a += 0x200000)
            mapPage2MB(ctx, 0xffff888000000000ULL + a, a, PAGE_PWT | PAGE_PCD);

        for (int i = 256; i < 512; i++)
            getOrCreateTable(ctx, ctx->pml4, i);

        loadPageTable(ctx);
    }
}
