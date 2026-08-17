#include "tss.hpp"
#include "gdt.hpp"
#include "kernel/MemoryCommon.hpp"
#include "kernel/BuddyAllocator.hpp"
#include "kernel/PageTable.hpp"

namespace Hardware {

    static TSS::Entry g_tss[TSS::MAX_CORES];
    static uint8_t g_ist1_stack[TSS::MAX_CORES][4096] __attribute__((aligned(16)));
    static uint8_t g_ist2_stack[TSS::MAX_CORES][4096] __attribute__((aligned(16)));

    static int getCoreId() {
        volatile uint32_t *lapic = (volatile uint32_t *)Memory::phys_to_virt(0xFEE00000);
        return (lapic[0x20 / 4] >> 24) & 0xFF;
    }

    namespace TSS {

        static void initCore(int core) {
            Memory::set(&g_tss[core], 0, sizeof(Entry));

            g_tss[core].ist[IST_CRITICAL - 1] = (uint64_t)(g_ist1_stack[core] + sizeof(g_ist1_stack[core]));
            g_tss[core].ist[IST_NMI - 1]      = (uint64_t)(g_ist2_stack[core] + sizeof(g_ist2_stack[core]));
            g_tss[core].iomap_base = sizeof(Entry);

            uint64_t base  = (uint64_t)&g_tss[core];
            uint32_t limit = sizeof(Entry) - 1;

            int slot = GDT::TSS_BASE_IDX + core * 2;
            g_gdt[slot].raw  = (uint64_t)(limit & 0xFFFF);
            g_gdt[slot].raw |= (uint64_t)(base & 0xFFFFFF) << 16;
            g_gdt[slot].raw |= (uint64_t)0x89 << 40;
            g_gdt[slot].raw |= (uint64_t)((limit >> 16) & 0xF) << 48;
            g_gdt[slot].raw |= (uint64_t)((base >> 24) & 0xFF) << 56;
            g_gdt[slot + 1].raw = (base >> 32);

            asm volatile("ltr %0" : : "r"(GDT::tssSel(core)));
        }

        void init()   { initCore(0); }
        void initAP(int core) { initCore(core); }

        void setRsp0(uint64_t rsp) {
            g_tss[getCoreId()].rsp[0] = rsp;
        }
    }
}