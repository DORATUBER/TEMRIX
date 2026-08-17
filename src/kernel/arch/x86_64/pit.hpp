#pragma once

#include "common.hpp"

namespace Hardware{
    namespace PIT
    {
        constexpr uint32_t BASE_FREQ = 1193182;
        constexpr uint16_t CHANNEL0 = 0x40;
        constexpr uint16_t COMMAND = 0x43;
        void init(uint32_t hz = 100);
    }
}

void sleep_ms(uint32_t ms);