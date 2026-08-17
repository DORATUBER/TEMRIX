#include "BuddyAllocator.hpp"

namespace Memory {
    MemoryRegion* BuddyAllocator::findRegionByPhys(uint64_t physAddr, uint32_t& pageOut) {
        for (uint32_t r = 0; r < num_regions; r++) {
            MemoryRegion* region = &regions[r];
            uint64_t regionStartPhys = virt_to_phys((uint64_t)region->start);
            uint64_t regionEndPhys   = regionStartPhys + region->total_pages * PAGE_SIZE;

            if (physAddr < regionStartPhys || physAddr >= regionEndPhys) continue;

            pageOut = (uint32_t)((physAddr - regionStartPhys) / PAGE_SIZE);
            return region;
        }
        return nullptr;
    }

    bool BuddyAllocator::unlinkBlock(MemoryRegion* region, int order, uint32_t page) {
        void* target = region->start + (uint64_t)page * PAGE_SIZE;
        FreeBlock** list_ptr = &region->free_lists[order];

        while (*list_ptr) {
            if ((void*)*list_ptr == target) {
                *list_ptr = (*list_ptr)->next;
                return true;
            }
            list_ptr = &((*list_ptr)->next);
        }
        return false;
    }

    void BuddyAllocator::pushFreeBlock(MemoryRegion* region, int order, uint32_t page) {
        FreeBlock* block = (FreeBlock*)(region->start + (uint64_t)page * PAGE_SIZE);
        block->next = region->free_lists[order];
        region->free_lists[order] = block;
        region->page_order[page] = (uint8_t)order;
    }

    uint32_t BuddyAllocator::getBuddyPage(uint32_t page, int order) {
        return page ^ (1 << order);
    }

    bool BuddyAllocator::isAligned(uint32_t page, int order) {
        return (page & ((1 << order) - 1)) == 0;
    }

    void BuddyAllocator::init(BootInfo* info) {
        num_regions = 0;
        free_memory = 0;

        uint8_t*  mmap        = (uint8_t*)info->MemoryMap;
        uint32_t  num_entries = info->MemoryMapSize / info->DescriptorSize;

        for (uint32_t i = 0; i < num_entries && num_regions < MAX_REGIONS; i++) {
            uint8_t*  desc  = mmap + (i * info->DescriptorSize);
            uint32_t  type  = *(uint32_t*)(desc + 0);
            uint64_t  addr  = *(uint64_t*)(desc + 8);
            uint64_t  pages = *(uint64_t*)(desc + 24);

            if (type != 7) continue;

            uint64_t total_pages    = pages;
            uint64_t metadata_pages = (total_pages + PAGE_SIZE - 1) / PAGE_SIZE;
            if (metadata_pages >= total_pages) continue;

            regions[num_regions].start      = (uint8_t*)phys_to_virt(addr);
            regions[num_regions].total_pages = total_pages;
            regions[num_regions].page_order  = (uint8_t*)phys_to_virt(addr);

            for (uint32_t j = 0; j < total_pages; j++)
                regions[num_regions].page_order[j] = 0xFF;

            for (int j = 0; j <= MAX_ORDER; j++)
                regions[num_regions].free_lists[j] = nullptr;

            for (uint32_t j = 0; j < metadata_pages; j++)
                regions[num_regions].page_order[j] = 0xFF | 0x80;

            uint64_t page_idx = metadata_pages;
            while (page_idx < total_pages) {
                int order = MAX_ORDER;
                uint64_t block_pages = 1ull << order;

                while (order > 0 &&
                    (block_pages > (total_pages - page_idx) ||
                        (page_idx % block_pages) != 0)) {
                    order--;
                    block_pages = 1ull << order;
                }

                FreeBlock* block = (FreeBlock*)(regions[num_regions].start + page_idx * PAGE_SIZE);
                block->next = regions[num_regions].free_lists[order];
                regions[num_regions].free_lists[order] = block;
                regions[num_regions].page_order[page_idx] = (uint8_t)order;

                free_memory += block_pages * PAGE_SIZE;
                page_idx += block_pages;
            }

            num_regions++;
        }
    }

    void* BuddyAllocator::malloc(uint32_t size) {
        m_lock.acquire();
        void* p = mallocImplementation(size);
        m_lock.release();
        return p;
    }

    void BuddyAllocator::free(void* ptr) {
        m_lock.acquire();
        freeImplementation(ptr);
        m_lock.release();
    }

    void* BuddyAllocator::mallocAligned(uint32_t size, uint32_t align) {
        m_lock.acquire();
        uint64_t raw = (uint64_t)mallocImplementation(size + align);
        m_lock.release();
        return (void*)((raw + align - 1) & ~((uint64_t)(align - 1)));
    }
    
    uint64_t BuddyAllocator::getFreeMemory() {
        m_lock.acquire();
        uint64_t f = free_memory;
        m_lock.release();
        return f;
    }

    void* BuddyAllocator::mallocImplementation(uint32_t size) {
        if (size == 0) return nullptr;

        uint32_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        int order = 0;
        while ((1u << order) < pages_needed) order++;
        if (order > MAX_ORDER) return nullptr;

        for (uint32_t r = 0; r < num_regions; r++) {
            MemoryRegion* region = &regions[r];

            int current_order = order;
            while (current_order <= MAX_ORDER && region->free_lists[current_order] == nullptr)
                current_order++;
            if (current_order > MAX_ORDER) continue;

            FreeBlock* block = region->free_lists[current_order];
            region->free_lists[current_order] = block->next;

            uint32_t page = (uint32_t)(((uint8_t*)block - region->start) / PAGE_SIZE);

            while (current_order > order) {
                current_order--;
                pushFreeBlock(region, current_order, page + (1u << current_order));
            }

            region->page_order[page] = (uint8_t)(order | 0x80);
            free_memory -= (1u << order) * PAGE_SIZE;
            return (void*)block;
        }

        return nullptr;
    }

    void BuddyAllocator::freeImplementation(void* ptr) {
        if (!ptr) return;

        uint32_t page;
        MemoryRegion* region = findRegionByPhys(virt_to_phys((uint64_t)ptr), page);
        if (!region) return;

        uint8_t entry = region->page_order[page];
        if (entry == 0xFF || !(entry & 0x80)) return;

        int order = entry & 0x7F;
        region->page_order[page] = (uint8_t)order;

        int original_order = order;

        while (order < MAX_ORDER) {
            uint32_t buddy_page = getBuddyPage(page, order);

            if (buddy_page >= region->total_pages ||
                region->page_order[buddy_page] != order ||
                !isAligned(page < buddy_page ? page : buddy_page, order + 1))
                break;

            unlinkBlock(region, order, buddy_page);

            if (buddy_page < page) page = buddy_page;
            order++;
        }

        pushFreeBlock(region, order, page);
        free_memory += (1u << original_order) * PAGE_SIZE;
    }
}