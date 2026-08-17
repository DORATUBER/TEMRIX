#pragma once
#include "kernel/common.hpp"
#include "kernel/MemoryCommon.hpp"
#include "kernel/BuddyAllocator.hpp"
#include "kernel/PageTable.hpp"
#include "kernel/stdlib/kVector.hpp"

namespace Memory
{
    static constexpr uint64_t VM_READ = 1ULL << 0;
    static constexpr uint64_t VM_WRITE = 1ULL << 1;
    static constexpr uint64_t VM_EXEC = 1ULL << 2;
    static constexpr uint64_t VM_USER = 1ULL << 3;
    static constexpr uint64_t VM_NOCACHE = 1ULL << 4;
    static constexpr uint64_t VM_DISCONTIG = 1ULL << 6;
    static constexpr uint64_t VM_MMIO = 1ULL << 7;
    static constexpr uint64_t VM_SHARED = 1ULL << 8;

    struct VMA
    {
        uint64_t start;
        uint64_t end;
        uint64_t flags;
    };

    class VMM
    {
    public:
        void init(BuddyAllocator *phys, PageTableContext *pt, uint64_t base);
        void init(BuddyAllocator *phys, PageTableContext *pt);
        void setBase(uint64_t base) { m_base = base; }

        uint64_t vmaCount() const { return m_vmas.size(); }
        uint64_t vmaCapacity() const { return m_vmas.capacity(); }

        uint64_t snapshot(VMA *out, uint64_t maxEntries) const;

        void *allocDiscontiguous(uint64_t size, uint64_t flags = VM_READ | VM_WRITE);

        void *allocAt(uint64_t virt, uint64_t size, uint64_t flags, bool zero = false);
        void *allocContigAt(uint64_t virt, uint64_t size, uint64_t flags, uint64_t *outPhys = nullptr);
        void *allocMmioAt(uint64_t virt, uint64_t phys, uint64_t size, uint64_t flags);
        void *mapSharedAt(uint64_t virt, const uint64_t *phys_pages, uint64_t page_count, uint64_t flags);

        void unmapShared(void *ptr, uint64_t page_count);

        void freeVirt(void *ptr, uint64_t size);
        void free(void *ptr, uint64_t size);
        void freeMmio(void *ptr, uint64_t size);

        void track(uint64_t start, uint64_t end, uint64_t flags);
        void destroyAll();
        void destroyMetadata() { m_vmas.destroy(); }

        bool find(uint64_t addr, VMA &out) const;


    private:
        BuddyAllocator *m_phys;
        PageTableContext *m_pt;
        uint64_t m_base;
        KVector<VMA> m_vmas;
        mutable Spinlock m_lock;

        uint64_t findGap(uint64_t size) const;
        bool overlapsLocked(uint64_t virt, uint64_t size) const; 
        void insert(uint64_t start, uint64_t end, uint64_t flags);
        void remove(uint64_t start);
        void unmapPages(uint64_t virt, uint64_t size, bool freePhys);
        static uint64_t toPageFlags(uint64_t flags);
    };

    static constexpr uint64_t KERNEL_VMM_BASE = 0xFFFFFF0000000000ULL;
}