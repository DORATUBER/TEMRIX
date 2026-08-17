#pragma once

#include "common.hpp"

struct InterruptFrame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

namespace Hardware{
    namespace ISR
    {
        void defaultISR(void* frame);
        void keyboard(void* frame);
        void timer(void* frame);
        void lapicSpuriousISR(void* frame);
        void lapicTimer(void* frame);

        void nmi(void* frame);
        void doubleFault(void* frame, uintptr_t error);
        void machineCheck(void* frame);

        extern "C" void syscallStub();
        extern "C" void apTimerStub();
    }
}