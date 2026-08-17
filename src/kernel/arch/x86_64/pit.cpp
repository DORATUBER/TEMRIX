#include "pit.hpp"
#include "io.hpp"

namespace Hardware{
    void PIT::init(uint32_t hz)
    {
        uint16_t divisor = 1193182 / hz;  
        outb(0x43, 0x36);                 
        outb(0x40, divisor & 0xFF);      
        outb(0x40, (divisor >> 8) & 0xFF);
    }
}