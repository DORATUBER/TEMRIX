#pragma once

#include <temrixstd/stdint.h>
#include <temrixstd/stddef.h>
#include <temrixstd/stdbool.h>
#include <temrixstd/sys/mman.h>

namespace Heap
{
    struct BlockHeader
    {
        size_t size; 
        bool free;
        BlockHeader *next;
        BlockHeader *prev;
    };

    static constexpr size_t Alignment = 16;
    static constexpr size_t PageSize = 4096;
    static constexpr size_t MinGrow = 64 * 1024; 

    static inline size_t AlignUp(size_t n, size_t align)
    {
        return (n + align - 1) & ~(align - 1);
    }

    struct HeapState
    {
        BlockHeader *head = nullptr;
        BlockHeader *tail = nullptr;
    };

    inline HeapState g_heap;

    static inline bool GrowHeap(size_t minSize)
    {
        size_t growSize = minSize > MinGrow ? minSize : MinGrow;
        growSize = AlignUp(growSize + sizeof(BlockHeader), PageSize);

        uint64_t region = Syscall::Memory::Map(growSize);
        if (!region)
            return false;

        BlockHeader *block = (BlockHeader *)region;
        block->size = growSize - sizeof(BlockHeader);
        block->free = true;
        block->next = nullptr;
        block->prev = g_heap.tail;

        if (g_heap.tail)
            g_heap.tail->next = block;
        else
            g_heap.head = block;

        g_heap.tail = block;
        return true;
    }

    static inline void Split(BlockHeader *block, size_t size)
    {
        if (block->size < size + sizeof(BlockHeader) + Alignment)
            return;

        BlockHeader *newBlock = (BlockHeader *)((uint8_t *)block + sizeof(BlockHeader) + size);
        newBlock->size = block->size - size - sizeof(BlockHeader);
        newBlock->free = true;
        newBlock->next = block->next;
        newBlock->prev = block;

        if (block->next)
            block->next->prev = newBlock;
        else
            g_heap.tail = newBlock;

        block->next = newBlock;
        block->size = size;
    }

    static inline void Coalesce(BlockHeader *block)
    {
        if (block->next && block->next->free)
        {
            BlockHeader *next = block->next;
            block->size += sizeof(BlockHeader) + next->size;
            block->next = next->next;

            if (block->next)
                block->next->prev = block;
            else
                g_heap.tail = block;
        }

        if (block->prev && block->prev->free)
            Coalesce(block->prev);
    }
}

static inline void *malloc(size_t size)
{
    if (size == 0)
        return nullptr;

    size = Heap::AlignUp(size, Heap::Alignment);

    for (Heap::BlockHeader *block = Heap::g_heap.head; block; block = block->next)
    {
        if (block->free && block->size >= size)
        {
            Heap::Split(block, size);
            block->free = false;
            return (void *)((uint8_t *)block + sizeof(Heap::BlockHeader));
        }
    }

    if (!Heap::GrowHeap(size))
        return nullptr;

    Heap::BlockHeader *block = Heap::g_heap.tail;
    Heap::Split(block, size);
    block->free = false;
    return (void *)((uint8_t *)block + sizeof(Heap::BlockHeader));
}

static inline void free(void *ptr)
{
    if (!ptr)
        return;

    Heap::BlockHeader *block = (Heap::BlockHeader *)((uint8_t *)ptr - sizeof(Heap::BlockHeader));
    block->free = true;
    Heap::Coalesce(block);
}

static inline void *calloc(size_t count, size_t size)
{
    size_t total = count * size;
    void *ptr = malloc(total);
    if (ptr)
        ::Memory::Set(ptr, 0, total);
    return ptr;
}

static inline void *realloc(void *ptr, size_t newSize)
{
    if (!ptr)
        return malloc(newSize);

    if (newSize == 0)
    {
        free(ptr);
        return nullptr;
    }

    Heap::BlockHeader *block = (Heap::BlockHeader *)((uint8_t *)ptr - sizeof(Heap::BlockHeader));
    size_t alignedNew = Heap::AlignUp(newSize, Heap::Alignment);

    if (block->size >= alignedNew)
    {
        Heap::Split(block, alignedNew);
        return ptr;
    }

    void *newPtr = malloc(newSize);
    if (!newPtr)
        return nullptr;

    size_t copySize = block->size < newSize ? block->size : newSize;
    ::Memory::Copy(newPtr, ptr, copySize);
    free(ptr);
    return newPtr;
}
