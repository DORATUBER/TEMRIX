#pragma once
#include "common.hpp"
#include "spinlock.hpp"

namespace Memory
{
    void set(void *dest, uint8_t value, size_t count);
    void copy(void *dest, const void *src, size_t count);
    int compare(const void *a, const void *b, size_t count);

    static constexpr uint64_t DIRECT_MAP_BASE = 0xffff888000000000ULL;

    inline uint64_t phys_to_virt(uint64_t phys) { return phys + DIRECT_MAP_BASE; }
    inline uint64_t virt_to_phys(uint64_t virt) { return virt - DIRECT_MAP_BASE; }

    struct FreeBlock
    {
        FreeBlock *next;
    };

    static constexpr uint32_t PAGE_SIZE = 4096;
    static constexpr int MAX_ORDER = 14;
    static constexpr uint32_t MAX_REGIONS = 32;

    struct MemoryRegion
    {
        uint8_t *start;
        uint32_t total_pages;
        FreeBlock *free_lists[MAX_ORDER + 1];
        uint8_t *page_order;
    };
}