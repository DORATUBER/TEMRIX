#include "timer.hpp"
volatile uint64_t g_ticks = 0;

namespace Hardware {
    uint64_t ticks() { return g_ticks; }
}