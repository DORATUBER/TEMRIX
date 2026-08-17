#include "SlabAllocator.hpp"
#include "Serial.hpp"

namespace Memory
{
    constexpr uint32_t SlabAllocator::CLASS_SIZES[];

    void SlabAllocator::init(BuddyAllocator *buddy)
    {
        m_buddy = buddy;
        for (uint32_t i = 0; i < NUM_CLASSES; i++)
            m_classes[i].pages = nullptr;
    }

    int SlabAllocator::classFor(uint32_t size, uint32_t align)
    {
        for (uint32_t i = 0; i < NUM_CLASSES; i++)
        {
            if (CLASS_SIZES[i] >= size && CLASS_SIZES[i] >= align)
                return (int)i;
        }
        return -1;
    }

    SlabAllocator::SlabPageHeader *SlabAllocator::createPageLocked(int classIdx)
    {
        void *raw = m_buddy->malloc(PAGE_SIZE);
        if (!raw)
            return nullptr;

        uint32_t chunkSize = CLASS_SIZES[classIdx];
        uint32_t numChunks = PAGE_SIZE / chunkSize; 

        uint8_t *base = (uint8_t *)raw;
        SlabPageHeader *header = (SlabPageHeader *)base;
        header->magic = SLAB_MAGIC;
        header->sizeClassIdx = (uint32_t)classIdx;
        header->usedCount = 0;
        header->freeList = nullptr;
        header->nextPage = m_classes[classIdx].pages;
        m_classes[classIdx].pages = header;

        for (uint32_t i = 1; i < numChunks; i++)
        {
            FreeChunk *chunk = (FreeChunk *)(base + (uint64_t)i * chunkSize);
            chunk->next = header->freeList;
            header->freeList = chunk;
        }

        return header;
    }

    void *SlabAllocator::allocFromClassLocked(int classIdx)
    {
        SlabClass &cls = m_classes[classIdx];

        SlabPageHeader *page = cls.pages;
        while (page && !page->freeList)
            page = page->nextPage;

        if (!page)
        {
            page = createPageLocked(classIdx);
            if (!page)
                return nullptr;
        }

        FreeChunk *chunk = page->freeList;
        page->freeList = chunk->next;
        page->usedCount++;
        return (void *)chunk;
    }

    void *SlabAllocator::malloc(uint32_t size)
    {
        if (size == 0)
            return nullptr;

        int classIdx = classFor(size, 1);
        if (classIdx < 0)
            return m_buddy->malloc(size); 

        m_lock.acquire();
        void *p = allocFromClassLocked(classIdx);
        m_lock.release();
        return p;
    }

    void *SlabAllocator::mallocAligned(uint32_t size, uint32_t align)
    {
        if (align <= 1)
            return malloc(size);

        if (align > PAGE_SIZE)
            return nullptr;

        int classIdx = classFor(size, align);
        if (classIdx < 0)
        {
            return m_buddy->malloc(size);
        }

        m_lock.acquire();
        void *p = allocFromClassLocked(classIdx);
        m_lock.release();
        return p;
    }

    void SlabAllocator::free(void *ptr)
    {
        if (!ptr)
            return;

        if (((uint64_t)ptr % PAGE_SIZE) == 0)
        {
            m_buddy->free(ptr);
            return;
        }

        uint64_t pageBase = (uint64_t)ptr & ~((uint64_t)PAGE_SIZE - 1);
        SlabPageHeader *header = (SlabPageHeader *)pageBase;

        if (header->magic != SLAB_MAGIC)
        {
            Serial::printf("[slab] CORRUPT FREE: ptr=%p pageBase=%p magic=0x%x (expected 0x%x)\n",
                           ptr, (void *)pageBase, header->magic, SLAB_MAGIC);
            Serial::render();
            for (;;)
                asm volatile("hlt");
        }

        if (header->sizeClassIdx >= NUM_CLASSES)
        {
            Serial::printf("[slab] CORRUPT FREE: ptr=%p pageBase=%p bad sizeClassIdx=%u\n",
                           ptr, (void *)pageBase, header->sizeClassIdx);
            Serial::render();
            for (;;)
                asm volatile("hlt");
        }

        m_lock.acquire();
        FreeChunk *chunk = (FreeChunk *)ptr;
        chunk->next = header->freeList;
        header->freeList = chunk;
        header->usedCount--;
        m_lock.release();
    }
}