#include "PageTable.hpp"

namespace Memory
{
    static uint64_t* allocTable(PageTableContext* ctx) {
        uint64_t* table = (uint64_t*)ctx->alloc->malloc(4096);
        Memory::set(table, 0, 4096);
        return table;
    }

    static uint64_t* getOrCreateTable(PageTableContext* ctx,
                                    uint64_t* parent,
                                    uint16_t index,
                                    uint64_t flags = 0) {
        if (parent[index] & PAGE_PRESENT)
            return (uint64_t*)phys_to_virt(parent[index] & ~0xFFFULL);

        uint64_t* table = allocTable(ctx);
        uint64_t  phys  = virt_to_phys((uint64_t)table);
        parent[index]   = phys | PAGE_PRESENT | PAGE_WRITABLE | flags;
        return table;
    }

    void mapPage2MB(PageTableContext* ctx,
                    uint64_t virt,
                    uint64_t phys,
                    uint64_t flags) {
        virt &= ~0x1FFFFFULL;
        phys &= ~0x1FFFFFULL;

        uint16_t pml4_idx = (virt >> 39) & 0x1FF;
        uint16_t pdpt_idx = (virt >> 30) & 0x1FF;
        uint16_t pd_idx   = (virt >> 21) & 0x1FF;

        uint64_t* pdpt = getOrCreateTable(ctx, ctx->pml4, pml4_idx);
        uint64_t* pd   = getOrCreateTable(ctx, pdpt, pdpt_idx);

        pd[pd_idx] = phys | flags | PAGE_PRESENT | PAGE_HUGE;
    }

    void mapPage4KB(PageTableContext* ctx,
                    uint64_t virt,
                    uint64_t phys,
                    uint64_t flags) {
        virt &= ~0xFFFULL;
        phys &= ~0xFFFULL;

        uint16_t pml4_idx = (virt >> 39) & 0x1FF;
        uint16_t pdpt_idx = (virt >> 30) & 0x1FF;
        uint16_t pd_idx   = (virt >> 21) & 0x1FF;
        uint16_t pt_idx   = (virt >> 12) & 0x1FF;

        uint64_t tableFlags = (flags & PAGE_USER) ? PAGE_USER : 0;

        uint64_t* pdpt = getOrCreateTable(ctx, ctx->pml4, pml4_idx, tableFlags);
        uint64_t* pd   = getOrCreateTable(ctx, pdpt, pdpt_idx, tableFlags);
        uint64_t* pt   = getOrCreateTable(ctx, pd, pd_idx, tableFlags);

        pt[pt_idx] = phys | flags | PAGE_PRESENT;
    }

    uint64_t virtToPhys(PageTableContext* ctx, uint64_t virt) {
        uint16_t pml4_idx = (virt >> 39) & 0x1FF;
        uint16_t pdpt_idx = (virt >> 30) & 0x1FF;
        uint16_t pd_idx   = (virt >> 21) & 0x1FF;
        uint16_t pt_idx   = (virt >> 12) & 0x1FF;

        if (!(ctx->pml4[pml4_idx] & PAGE_PRESENT)) return 0;
        uint64_t* pdpt = (uint64_t*)phys_to_virt(ctx->pml4[pml4_idx] & ~0xFFFULL);

        if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;
        uint64_t* pd = (uint64_t*)phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);

        if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;
        if (pd[pd_idx] & PAGE_HUGE)
            return (pd[pd_idx] & ~0x1FFFFFULL) + (virt & 0x1FFFFFULL);
        uint64_t* pt = (uint64_t*)phys_to_virt(pd[pd_idx] & ~0xFFFULL);

        if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;
        return (pt[pt_idx] & ~0xFFFULL) + (virt & 0xFFFULL);
    }

    void initPageTables(PageTableContext* ctx,
                        BootInfo* info) {
        ctx->pml4 = allocTable(ctx);

        for (uint64_t a = 0; a < 0x100000000ULL; a += 0x200000)
            mapPage2MB(ctx, a, a, 0);

        mapPage2MB(ctx, 0xFFFF800000100000, 0x100000, 0);

        uint8_t* mmap        = (uint8_t*)info->MemoryMap;
        uint32_t num_entries = info->MemoryMapSize / info->DescriptorSize;

        for (uint32_t i = 0; i < num_entries; i++) {
            uint8_t* desc  = mmap + (i * info->DescriptorSize);
            uint32_t type  = *(uint32_t*)(desc + 0);
            uint64_t addr  = *(uint64_t*)(desc + 8);
            uint64_t pages = *(uint64_t*)(desc + 24);
            uint64_t size  = pages * 4096;

            if (type != 11 && type != 12)
                continue;

            uint64_t start = addr & ~0x1FFFFFULL;
            uint64_t end   = (addr + size + 0x1FFFFFULL) & ~0x1FFFFFULL;

            for (uint64_t a = start; a < end; a += 0x200000)
                mapPage2MB(ctx, a, a, PAGE_PWT | PAGE_PCD);
        }

        loadPageTable(ctx);
    }

    void mapMMIO(PageTableContext* ctx,
                 uint64_t phys,
                 uint64_t size) {
        uint64_t start = phys & ~0x1FFFFFULL;
        uint64_t end   = (phys + size + 0x1FFFFFULL) & ~0x1FFFFFULL;

        for (uint64_t a = start; a < end; a += 0x200000)
            mapPage2MB(ctx, a, a, PAGE_PWT | PAGE_PCD);
    }

    void syncKernelMappings(PageTableContext* proc_ctx, PageTableContext* kernel_ctx) {
        for (int i = 256; i < 512; i++)
            proc_ctx->pml4[i] = kernel_ctx->pml4[i];
    }

    PageTableContext createProcessPageTable(PageTableContext* kernel_ctx, BuddyAllocator* alloc) {
        uint64_t* pml4 = (uint64_t*)alloc->malloc(4096);
        Memory::set(pml4, 0, 4096);

        for (int i = 0; i < 256; i++)
            pml4[i] = 0;

        for (int i = 256; i < 512; i++)
            pml4[i] = kernel_ctx->pml4[i];

        PageTableContext ctx;
        ctx.pml4  = pml4;
        ctx.alloc = alloc;
        return ctx;
    }

    void freePageTable(PageTableContext* ctx, BuddyAllocator* alloc) {
        for (int pml4_idx = 0; pml4_idx < 256; pml4_idx++) {
            if (!(ctx->pml4[pml4_idx] & PAGE_PRESENT)) continue;
            uint64_t* pdpt = (uint64_t*)phys_to_virt(ctx->pml4[pml4_idx] & ~0xFFFULL);

            for (int pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++) {
                if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) continue;
                uint64_t* pd = (uint64_t*)phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);

                for (int pd_idx = 0; pd_idx < 512; pd_idx++) {
                    if (!(pd[pd_idx] & PAGE_PRESENT)) continue;

                    if (pd[pd_idx] & PAGE_HUGE) continue; 

                    uint64_t* pt = (uint64_t*)phys_to_virt(pd[pd_idx] & ~0xFFFULL);
                    alloc->free((void*)pt);
                }

                alloc->free((void*)pd);
            }

            alloc->free((void*)pdpt);
        }

        alloc->free((void*)ctx->pml4);
        ctx->pml4 = nullptr;
    }
}