#pragma once

#include <temrixstd.h>

namespace AMD {
    class GpuMemory {
    public:
        void init(void* bar0, void* bar2, void* bar5) {
            m_vram     = (volatile uint32_t*)bar0;
            m_doorbell = (volatile uint32_t*)bar2;
            m_mmio     = (volatile uint32_t*)bar5;
        }

        uint32_t reg_read(uint32_t reg) {
            return m_mmio[reg];
        }

        void reg_write(uint32_t reg, uint32_t val) {
            m_mmio[reg] = val;
        }

        void vram_read(uint64_t pos, uint8_t* dst, uint32_t size)
        {
            uint32_t prev_hi = ~0u;
            for (uint32_t i = 0; i < size / 4; i++)
            {
                uint64_t addr = pos + i * 4;
                uint32_t lo   = (uint32_t)(addr & 0xFFFFFFFF) | 0x80000000;
                uint32_t hi   = (uint32_t)(addr >> 31);

                m_mmio[MM_INDEX] = lo;
                if (hi != prev_hi)
                {
                    m_mmio[MM_INDEX_HI] = hi;
                    prev_hi = hi;
                }

                uint32_t val = m_mmio[MM_DATA];
                dst[i * 4 + 0] = (val >>  0) & 0xFF;
                dst[i * 4 + 1] = (val >>  8) & 0xFF;
                dst[i * 4 + 2] = (val >> 16) & 0xFF;
                dst[i * 4 + 3] = (val >> 24) & 0xFF;
            }
        }

        void vram_write(uint64_t pos, const uint8_t* src, uint32_t size)
        {
            uint32_t prev_hi = ~0u;
            for (uint32_t i = 0; i < size / 4; i++)
            {
                uint64_t addr = pos + i * 4;
                uint32_t lo   = (uint32_t)(addr & 0xFFFFFFFF) | 0x80000000;
                uint32_t hi   = (uint32_t)(addr >> 31);

                m_mmio[MM_INDEX] = lo;
                if (hi != prev_hi)
                {
                    m_mmio[MM_INDEX_HI] = hi;
                    prev_hi = hi;
                }

                uint32_t val = ((uint32_t)src[i * 4 + 0] <<  0) |
                               ((uint32_t)src[i * 4 + 1] <<  8) |
                               ((uint32_t)src[i * 4 + 2] << 16) |
                               ((uint32_t)src[i * 4 + 3] << 24);
                m_mmio[MM_DATA] = val;
            }
        }

        void doorbell_write(uint32_t offset, uint64_t val) {
            volatile uint64_t* db = (volatile uint64_t*)m_doorbell;
            db[offset >> 3] = val;
        }

    private:
        static constexpr uint32_t MM_INDEX    = 0x0000;
        static constexpr uint32_t MM_INDEX_HI = 0x0006;
        static constexpr uint32_t MM_DATA     = 0x0001;

        volatile uint32_t* m_vram;
        volatile uint32_t* m_doorbell;
        volatile uint32_t* m_mmio;
    };

}