#include "lapic.hpp"

uint32_t g_lapic_ticks_per_quantum = 0;

namespace Hardware
{
    namespace LAPIC
    {
        void init()
        {
            uint32_t lo, hi;
            asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0x1B));
            lo |= (1 << 11); 
            asm volatile("wrmsr" ::"c"(0x1B), "a"(lo), "d"(hi));

            write(REG_SVR, SVR_ENABLE | Vector::LAPIC_SPURIOUS);
        }
        void initAP() {
            
            uint32_t lo, hi;
            asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0x1B));
            lo |= (1 << 11);
            asm volatile("wrmsr" :: "c"(0x1B), "a"(lo), "d"(hi));

            
            write(REG_SVR, SVR_ENABLE | Vector::LAPIC_SPURIOUS);
        }
    }
}