#pragma once

#include "common.hpp"
#include "kernel/MemoryCommon.hpp"
#include "kernel/BuddyAllocator.hpp"
#include "kernel/PageTable.hpp"
#include "vector.hpp"

namespace Hardware
{
    namespace LAPIC
    {
        constexpr uint32_t BASE = 0xFEE00000;
        constexpr uint32_t REG_SVR = 0xF0; 
        constexpr uint32_t REG_EOI = 0xB0; 
        constexpr uint32_t SVR_ENABLE = (1 << 8);

        constexpr uint32_t MSI_ADDRESS = 0xFEE00000;
        constexpr uint32_t MSI_DATA_LEVEL = (1 << 14); 
        constexpr uint32_t MSI_DATA_ASSERT = (1 << 15);

        constexpr uint32_t REG_ICR_LO = 0x300; 
        constexpr uint32_t REG_ICR_HI = 0x310; 

        inline volatile uint32_t *reg(uint32_t offset)
        {
            return (volatile uint32_t *)(Memory::phys_to_virt((uint64_t)BASE) + offset);
        }

        inline void write(uint32_t offset, uint32_t val)
        {
            *reg(offset) = val;
        }

        inline uint32_t read(uint32_t offset)
        {
            return *reg(offset);
        }

        inline void eoi()
        {
            write(REG_EOI, 0);
        }

        void init();
        void initAP();

        inline void sendSelf(uint8_t vector)
        {
            while (read(REG_ICR_LO) & (1 << 12))
                asm volatile("pause");

            write(REG_ICR_HI, 0);
            write(REG_ICR_LO, (1 << 18) | vector);
        }

        static inline uint32_t getApicId()
        {
            volatile uint32_t *lapic =
                (volatile uint32_t *)Memory::phys_to_virt(0xFEE00000);

            return lapic[0x20 / 4] >> 24;
        }
    }
} 