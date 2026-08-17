#include "gdt.hpp"

namespace Hardware {
    #define GDT_ACCESS_PRESENT     0x80  
    #define GDT_ACCESS_DPL0        0x00 
    #define GDT_ACCESS_DPL3        0x60  
    #define GDT_ACCESS_DESCRIPTOR  0x10 
    #define GDT_ACCESS_EXECUTABLE  0x08  
    #define GDT_ACCESS_RW          0x02  

    #define GDT_FLAG_GRANULARITY   0x8   
    #define GDT_FLAG_SIZE_32       0x4 
    #define GDT_FLAG_LONG_MODE     0x2   

    #define KERNEL_CODE_SELECTOR   0x08
    #define KERNEL_DATA_SELECTOR   0x10

    #define GDT_ENTRY(base, limit, access, flags)                          \
        ( ((uint64_t)((limit) & 0xFFFFULL))                                \
        | (((uint64_t)(base)  & 0xFFFFFFULL)        << 16)                 \
        | (((uint64_t)(access) & 0xFFULL)            << 40)                \
        | ((((uint64_t)(limit) >> 16) & 0xFULL)      << 48)                \
        | (((uint64_t)(flags) & 0xFULL)               << 52)               \
        | ((((uint64_t)(base) >> 24) & 0xFFULL)      << 56) )

    #define GDT_NULL_ENTRY 0ULL

    GDT::Entry g_gdt[GDT::GDT_SIZE];
    GDT::Register g_gdtr;

    static void loadGdtAndReloadSegments() {
        asm volatile(
            "lgdt %0\n\t"
            "push %1\n\t"
            "lea  1f(%%rip), %%rax\n\t"
            "push %%rax\n\t"
            "lretq\n\t"
            "1:\n\t"
            "mov %2, %%ax\n\t"
            "mov %%ax,  %%ds\n\t"
            "mov %%ax,  %%es\n\t"
            "mov %%ax,  %%ss\n\t"
            "xor %%ax,  %%ax\n\t"
            "mov %%ax,  %%fs\n\t"
            "mov %%ax,  %%gs\n\t"
            :
            : "m"(g_gdtr), "i"(KERNEL_CODE_SELECTOR), "i"(KERNEL_DATA_SELECTOR)
            : "rax", "memory");
    }

    void GDT::init() {
        g_gdt[0].raw = GDT_NULL_ENTRY;

        g_gdt[1].raw = GDT_ENTRY(0, 0xFFFFF,
            GDT_ACCESS_PRESENT | GDT_ACCESS_DESCRIPTOR | GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
            GDT_FLAG_GRANULARITY | GDT_FLAG_LONG_MODE);

        g_gdt[2].raw = GDT_ENTRY(0, 0xFFFFF,
            GDT_ACCESS_PRESENT | GDT_ACCESS_DESCRIPTOR | GDT_ACCESS_RW,
            GDT_FLAG_GRANULARITY | GDT_FLAG_SIZE_32);

        g_gdt[3].raw = GDT_ENTRY(0, 0xFFFFF,
            GDT_ACCESS_PRESENT | GDT_ACCESS_DPL3 | GDT_ACCESS_DESCRIPTOR | GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
            GDT_FLAG_GRANULARITY | GDT_FLAG_LONG_MODE);

        g_gdt[4].raw = GDT_ENTRY(0, 0xFFFFF,
            GDT_ACCESS_PRESENT | GDT_ACCESS_DPL3 | GDT_ACCESS_DESCRIPTOR | GDT_ACCESS_RW,
            GDT_FLAG_GRANULARITY | GDT_FLAG_LONG_MODE);

        for (int i = TSS_BASE_IDX; i < GDT_SIZE; i++)
            g_gdt[i].raw = GDT_NULL_ENTRY;

        g_gdtr.limit = sizeof(g_gdt) - 1;
        g_gdtr.base  = (uint64_t)&g_gdt[0];

        loadGdtAndReloadSegments();
    }

    void GDT::loadOnCurrentCPU() {
        loadGdtAndReloadSegments();
    }
}