#include "loader.hpp"
#include "KernelState.hpp"
#include "Serial.hpp" 

extern Kernel kernel;

#define TRX_DEBUG 1

#if TRX_DEBUG
#define TRX_LOG(...) Serial::printf(__VA_ARGS__)
#else
#define TRX_LOG(...) do {} while (0)
#endif

static constexpr uint64_t TRX_STACK_TOP = 0x7FFFFFFFE000ULL;

static void writeU64AtVA(Memory::PageTableContext *pt, uint64_t va, uint64_t value)
{
    uint8_t bytes[8];
    for (int i = 0; i < 8; i++)
        bytes[i] = (uint8_t)(value >> (8 * i));

    uint64_t currentPage = (uint64_t)-1;
    uint8_t *dstBase = nullptr;

    for (int i = 0; i < 8; i++)
    {
        uint64_t addr = va + i;
        uint64_t page = addr >> 12;
        if (page != currentPage)
        {
            uint64_t phys = Memory::virtToPhys(pt, addr);
            dstBase = (uint8_t *)Memory::phys_to_virt(phys) - (addr & 0xFFF);
            currentPage = page;
        }
        dstBase[addr & 0xFFF] = bytes[i];
    }
}

bool Loader::spawn(const KernelSpawnInfo* info, Task** outTask) {
    TRX_LOG("[loader] spawn: entry=0x%llx bias=0x%llx numSections=%u numRelocs=%u stackSize=0x%llx\n",
            info->entry, info->bias, info->numSections, info->numRelocs, info->stackSize);

    auto* pt = new Memory::PageTableContext(
        Memory::createProcessPageTable(&kernel.ptCtx, &kernel.allocator));
    if (!pt) {
        TRX_LOG("[loader] spawn: page table alloc failed\n");
        return false;
    }

    Task* t = Task::createUserShell(pt);
    if (!t) {
        TRX_LOG("[loader] spawn: createUserShell failed\n");
        Memory::freePageTable(pt, &kernel.allocator);
        delete pt;
        return false;
    }

    uint64_t highestEnd = 0;

    for (uint32_t i = 0; i < info->numSections; i++) {
        const KernelSection& s = info->sections[i];

        bool needsZeroFill = s.memSize > s.fileSize;

        TRX_LOG("[loader] mapping section[%u] dst=0x%llx memSize=0x%llx fileSize=0x%llx vmFlags=0x%llx zeroFill=%d\n",
                i, s.dst, s.memSize, s.fileSize, s.vmFlags, (int)needsZeroFill);

        if (!t->userVmm->allocAt(s.dst, s.memSize, s.vmFlags | Memory::VM_USER, needsZeroFill)) {
            TRX_LOG("[loader] spawn: allocAt failed for section[%u]\n", i);
            t->destroy();
            return false;
        }

        if (s.fileSize > 0) {
            uint64_t currentPage = (uint64_t)-1;
            uint8_t* dstBase     = nullptr;

            for (uint64_t off = 0; off < s.fileSize; off++) {
                uint64_t page = (s.dst + off) >> 12;
                if (page != currentPage) {
                    uint64_t phys = Memory::virtToPhys(pt, s.dst + off);
                    dstBase = (uint8_t*)Memory::phys_to_virt(phys) - ((s.dst + off) & 0xFFF);
                    currentPage = page;
                }
                dstBase[(s.dst + off) & 0xFFF] = s.src[off];
            }
        }
        uint64_t end = s.dst + s.memSize;
        if (end > highestEnd) highestEnd = end;
    }

    for (uint32_t i = 0; i < info->numRelocs; i++) {
        const KernelReloc& r = info->relocs[i];
        uint64_t target = r.offset + info->bias;
        bool found = false;

        for (uint32_t s = 0; s < info->numSections; s++) {
            const KernelSection& sec = info->sections[s];
            if (target >= sec.dst &&
                target < sec.dst + sec.memSize &&
                sec.memSize - (target - sec.dst) >= sizeof(uint64_t))
            {
                uint64_t value = info->bias + (uint64_t)r.addend;
                writeU64AtVA(pt, target, value);

                TRX_LOG("[loader] reloc[%u] section=%u target=0x%llx addend=0x%llx -> 0x%llx\n",
                        i, s, target, (uint64_t)r.addend, value);

                found = true;
                break;
            }
        }

        if (!found) {
            TRX_LOG("[loader] spawn: reloc[%u] target=0x%llx not in any mapped section\n", i, target);
            t->destroy();
            return false;
        }
    }

    uint64_t stackAligned = (info->stackSize + 0xFFFULL) & ~0xFFFULL;
    uint64_t stackBase = TRX_STACK_TOP - stackAligned;

    TRX_LOG("[loader] mapping stack base=0x%llx size=0x%llx\n", stackBase, stackAligned);

    if (!t->userVmm->allocAt(stackBase, stackAligned,
                              Memory::VM_READ | Memory::VM_WRITE | Memory::VM_USER)) {
        TRX_LOG("[loader] spawn: stack allocAt failed\n");
        t->destroy();
        return false;
    }

    TRX_LOG("[loader] finalizeUser entry=0x%llx sp=0x%llx\n", info->entry, TRX_STACK_TOP - 8);

    if (!Task::finalizeUser(t, info->entry, TRX_STACK_TOP - 8)) {
        TRX_LOG("[loader] spawn: finalizeUser failed\n");
        t->destroy();
        return false;
    }

    uint64_t newBase = (highestEnd + 0xFFF) & ~0xFFFULL;
    TRX_LOG("[loader] spawn: OK, setBase=0x%llx\n", newBase);

    t->userVmm->setBase(newBase);
    *outTask = t;
    return true;
}