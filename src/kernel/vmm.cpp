#include "vmm.hpp"

namespace Memory
{
    void VMM::init(BuddyAllocator *phys, PageTableContext *pt, uint64_t base)
    {
        m_phys = phys;
        m_pt = pt;
        m_base = base;
        m_vmas.init(phys);
    }

    void VMM::init(BuddyAllocator *phys, PageTableContext *pt)
    {
        m_phys = phys;
        m_pt = pt;
        m_vmas.init(phys);
    }

    bool VMM::overlapsLocked(uint64_t virt, uint64_t size) const
    {
        uint64_t end = virt + size;
        for (uint64_t i = 0; i < m_vmas.size(); i++)
        {
            uint64_t oStart = m_vmas[i].start, oEnd = m_vmas[i].end;
            if (virt < oEnd && end > oStart)
                return true;
        }
        return false;
    }

    uint64_t VMM::snapshot(VMA *out, uint64_t maxEntries) const
    {
        m_lock.acquire();
        uint64_t total = m_vmas.size();
        uint64_t toCopy = (total < maxEntries) ? total : maxEntries;
        for (uint64_t i = 0; i < toCopy; i++)
            out[i] = m_vmas[i];
        m_lock.release();
        return total;
    }

    uint64_t VMM::findGap(uint64_t size) const
    {
        uint64_t cursor = m_base;
        for (uint64_t i = 0; i < m_vmas.size(); i++)
        {
            const VMA &v = m_vmas[i];
            if (v.start >= cursor + size)
                return cursor;
            if (v.end > cursor)
                cursor = v.end;
        }
        return cursor;
    }

    void VMM::insert(uint64_t start, uint64_t end, uint64_t flags)
    {
        VMA v{start, end, flags};
        m_vmas.push(v);

        uint64_t i = m_vmas.size() - 1;
        while (i > 0 && m_vmas[i].start < m_vmas[i - 1].start)
        {
            VMA tmp = m_vmas[i];
            m_vmas[i] = m_vmas[i - 1];
            m_vmas[i - 1] = tmp;
            i--;
        }
    }

    void VMM::remove(uint64_t start)
    {
        for (uint64_t i = 0; i < m_vmas.size(); i++)
        {
            if (m_vmas[i].start == start)
            {
                m_vmas.remove(i);
                return;
            }
        }
    }

    bool VMM::find(uint64_t addr, VMA &out) const
    {
        m_lock.acquire();
        for (uint64_t i = 0; i < m_vmas.size(); i++)
        {
            if (addr >= m_vmas[i].start && addr < m_vmas[i].end)
            {
                out = m_vmas[i];
                m_lock.release();
                return true;
            }
        }
        m_lock.release();
        return false;
    }

    uint64_t VMM::toPageFlags(uint64_t f)
    {
        uint64_t pf = PAGE_PRESENT;
        if (f & VM_WRITE)
            pf |= PAGE_WRITABLE;
        if (f & VM_USER)
            pf |= PAGE_USER;
        if (f & VM_NOCACHE)
            pf |= PAGE_PCD;
        return pf;
    }

    static bool isTableEmpty(uint64_t *table)
    {
        for (int i = 0; i < 512; i++)
        {
            if (table[i] & PAGE_PRESENT)
                return false;
        }
        return true;
    }

    void VMM::unmapPages(uint64_t virt, uint64_t size, bool freePhys)
    {
        uint64_t bytes = (size + 0xFFFULL) & ~0xFFFULL;
        for (uint64_t off = 0; off < bytes; off += 0x1000)
        {
            uint64_t page_virt = virt + off;
            uint16_t pml4_idx = (page_virt >> 39) & 0x1FF;
            uint16_t pdpt_idx = (page_virt >> 30) & 0x1FF;
            uint16_t pd_idx = (page_virt >> 21) & 0x1FF;
            uint16_t pt_idx = (page_virt >> 12) & 0x1FF;

            if (!(m_pt->pml4[pml4_idx] & PAGE_PRESENT))
                continue;
            uint64_t *pdpt = reinterpret_cast<uint64_t *>(phys_to_virt(m_pt->pml4[pml4_idx] & ~0xFFFULL));

            if (!(pdpt[pdpt_idx] & PAGE_PRESENT))
                continue;
            uint64_t *pd = reinterpret_cast<uint64_t *>(phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL));

            if (!(pd[pd_idx] & PAGE_PRESENT))
                continue;

            if (pd[pd_idx] & PAGE_HUGE)
            {
                if (freePhys)
                {
                    uint64_t phys = pd[pd_idx] & ~0x1FFFFFULL;   
                    m_phys->free(reinterpret_cast<void *>(phys_to_virt(phys)));
                }
                pd[pd_idx] = 0;
                asm volatile("invlpg (%0)" ::"r"(page_virt) : "memory");

                if (isTableEmpty(pd))
                {
                    uint64_t pdPhys = pdpt[pdpt_idx] & ~0xFFFULL;
                    pdpt[pdpt_idx] = 0;
                    m_phys->free(reinterpret_cast<void *>(phys_to_virt(pdPhys)));

                    if (isTableEmpty(pdpt))
                    {
                        uint64_t pdptPhys = m_pt->pml4[pml4_idx] & ~0xFFFULL;
                        m_pt->pml4[pml4_idx] = 0;
                        m_phys->free(reinterpret_cast<void *>(phys_to_virt(pdptPhys)));
                    }
                }
                continue;
            }

            uint64_t *pt = reinterpret_cast<uint64_t *>(phys_to_virt(pd[pd_idx] & ~0xFFFULL));

            if (!(pt[pt_idx] & PAGE_PRESENT))
                continue;

            if (freePhys)
            {
                uint64_t phys = pt[pt_idx] & ~0xFFFULL;
                m_phys->free(reinterpret_cast<void *>(phys_to_virt(phys)));
            }
            pt[pt_idx] = 0;
            asm volatile("invlpg (%0)" ::"r"(page_virt) : "memory");

            if (isTableEmpty(pt))
            {
                uint64_t ptPhys = pd[pd_idx] & ~0xFFFULL;
                pd[pd_idx] = 0;
                m_phys->free(reinterpret_cast<void *>(phys_to_virt(ptPhys)));

                if (isTableEmpty(pd))
                {
                    uint64_t pdPhys = pdpt[pdpt_idx] & ~0xFFFULL;
                    pdpt[pdpt_idx] = 0;
                    m_phys->free(reinterpret_cast<void *>(phys_to_virt(pdPhys)));

                    if (isTableEmpty(pdpt))
                    {
                        uint64_t pdptPhys = m_pt->pml4[pml4_idx] & ~0xFFFULL;
                        m_pt->pml4[pml4_idx] = 0;
                        m_phys->free(reinterpret_cast<void *>(phys_to_virt(pdptPhys)));
                    }
                }
            }
        }
    }

    void *VMM::allocDiscontiguous(uint64_t size, uint64_t flags)
    {
        m_lock.acquire();

        uint64_t aligned = (size + 0xFFFULL) & ~0xFFFULL;
        uint64_t virt = findGap(aligned);
        uint64_t pages = aligned >> 12;
        uint64_t page_flags = toPageFlags(flags);

        for (uint64_t i = 0; i < pages; i++)
        {
            void *pv = m_phys->malloc(0x1000);
            if (!pv)
            {
                for (uint64_t j = 0; j < i; j++)
                    unmapPages(virt + j * 0x1000, 0x1000, true);
                m_lock.release();
                return nullptr;
            }
            uint64_t phys = virt_to_phys((uint64_t)pv);
            mapPage4KB(m_pt, virt + i * 0x1000, phys, page_flags);
        }

        insert(virt, virt + aligned, flags | VM_DISCONTIG);

        m_lock.release();
        return reinterpret_cast<void *>(virt);
    }

    void *VMM::allocAt(uint64_t virt, uint64_t size, uint64_t flags, bool zero)
    {
        m_lock.acquire();

        uint64_t aligned = (size + 0xFFFULL) & ~0xFFFULL;

        if (overlapsLocked(virt, aligned))
        {
            m_lock.release();
            return nullptr;
        }

        uint64_t page_flags = toPageFlags(flags);
        uint64_t pages = aligned >> 12;
        for (uint64_t i = 0; i < pages; i++)
        {
            void *pv = m_phys->malloc(0x1000);
            if (!pv)
            {
                for (uint64_t j = 0; j < i; j++)
                    unmapPages(virt + j * 0x1000, 0x1000, true);
                m_lock.release();
                return nullptr;
            }
            if (zero) Memory::set(pv, 0, 0x1000);
            mapPage4KB(m_pt, virt + i * 0x1000, virt_to_phys((uint64_t)pv), page_flags);
        }

        insert(virt, virt + aligned, flags | VM_DISCONTIG);

        m_lock.release();
        return reinterpret_cast<void *>(virt);
    }

    void *VMM::allocContigAt(uint64_t virt, uint64_t size, uint64_t flags, uint64_t *outPhys)
    {
        m_lock.acquire();

        uint64_t aligned = (size + 0xFFFULL) & ~0xFFFULL;

        if (overlapsLocked(virt, aligned))
        {
            m_lock.release();
            return nullptr;
        }

        void *phys_virt = m_phys->malloc(aligned);
        if (!phys_virt)
        {
            m_lock.release();
            return nullptr;
        }

        uint64_t phys = virt_to_phys((uint64_t)phys_virt);
        if (outPhys)
            *outPhys = phys;

        uint64_t page_flags = toPageFlags(flags);
        uint64_t pages = aligned >> 12;
        for (uint64_t i = 0; i < pages; i++)
            mapPage4KB(m_pt, virt + i * 0x1000, phys + i * 0x1000, page_flags);

        insert(virt, virt + aligned, flags);

        m_lock.release();
        return reinterpret_cast<void *>(virt);
    }

    void *VMM::allocMmioAt(uint64_t virt, uint64_t phys, uint64_t size, uint64_t flags)
    {
        m_lock.acquire();

        uint64_t aligned = (size + 0xFFFULL) & ~0xFFFULL;

        if (overlapsLocked(virt, aligned))
        {
            m_lock.release();
            return nullptr;
        }

        uint64_t page_flags = PAGE_PRESENT | PAGE_PCD | PAGE_PWT;
        if (flags & VM_WRITE)
            page_flags |= PAGE_WRITABLE;
        if (flags & VM_USER)
            page_flags |= PAGE_USER;

        for (uint64_t off = 0; off < aligned; off += 0x1000)
            mapPage4KB(m_pt, virt + off, phys + off, page_flags);

        insert(virt, virt + aligned, flags | VM_MMIO);

        m_lock.release();
        return reinterpret_cast<void *>(virt);
    }

    void *VMM::mapSharedAt(uint64_t virt, const uint64_t *phys_pages, uint64_t page_count, uint64_t flags)
    {
        m_lock.acquire();

        uint64_t size = page_count << 12;

        if (overlapsLocked(virt, size))
        {
            m_lock.release();
            return nullptr;
        }

        uint64_t page_flags = toPageFlags(flags);
        for (uint64_t i = 0; i < page_count; i++)
            mapPage4KB(m_pt, virt + i * 0x1000, phys_pages[i], page_flags);

        insert(virt, virt + size, flags | VM_SHARED);

        m_lock.release();
        return reinterpret_cast<void *>(virt);
    }

    void VMM::unmapShared(void *ptr, uint64_t page_count)
    {
        m_lock.acquire();
        unmapPages(reinterpret_cast<uint64_t>(ptr), page_count << 12, false);
        remove(reinterpret_cast<uint64_t>(ptr));
        m_lock.release();
    }

    void VMM::freeVirt(void *ptr, uint64_t size)
    {
        m_lock.acquire();
        uint64_t virt = reinterpret_cast<uint64_t>(ptr);
        unmapPages(virt, size, false);
        remove(virt);
        m_lock.release();
    }

    void VMM::free(void *ptr, uint64_t size)
    {
        m_lock.acquire();
        uint64_t virt = reinterpret_cast<uint64_t>(ptr);
        unmapPages(virt, size, true);
        remove(virt);
        m_lock.release();
    }

    void VMM::freeMmio(void *ptr, uint64_t size)
    {
        freeVirt(ptr, size); 
    }

    void VMM::destroyAll()
    {
        m_lock.acquire();
        for (uint64_t i = 0; i < m_vmas.size(); i++)
        {
            const VMA &vma = m_vmas[i];
            if (vma.flags & (VM_MMIO | VM_SHARED))
                continue;
            unmapPages(vma.start, vma.end - vma.start, true);
        }
        m_vmas.destroy();
        m_lock.release();
    }

    void VMM::track(uint64_t start, uint64_t end, uint64_t flags)
    {
        m_lock.acquire();
        insert(start, end, flags);
        m_lock.release();
    }
}