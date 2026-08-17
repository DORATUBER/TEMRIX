#pragma once
#include "common.hpp"

namespace Hardware {
    extern "C" volatile uint64_t g_ticks;
    uint64_t ticks();
}