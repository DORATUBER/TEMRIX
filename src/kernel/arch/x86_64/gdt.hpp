#pragma once
#include "common.hpp"

namespace Hardware { 
    namespace GDT {
        constexpr int MAX_CORES = 8;

        constexpr uint16_t KERNEL_CODE = 0x08;
        constexpr uint16_t KERNEL_DATA = 0x10;
        constexpr uint16_t USER_CODE   = 0x1B;
        constexpr uint16_t USER_DATA   = 0x23;
 
        constexpr uint16_t TSS_BASE_IDX = 5;
        constexpr int      GDT_SIZE     = TSS_BASE_IDX + MAX_CORES * 2;

        inline uint16_t tssSel(int core) {
            return (TSS_BASE_IDX + core * 2) * 8;
        }

        struct __attribute__((packed)) Entry {
            uint64_t raw;
        };

        struct __attribute__((packed)) Register {
            uint16_t limit;
            uint64_t base;
        };

        void init();
        void loadOnCurrentCPU();
    }

    extern GDT::Entry g_gdt[GDT::GDT_SIZE];
    extern GDT::Register g_gdtr;
}