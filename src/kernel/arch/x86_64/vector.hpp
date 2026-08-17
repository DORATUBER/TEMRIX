#pragma once

#include "common.hpp"

namespace Hardware{
    namespace Vector
    {
        constexpr uint8_t DIVIDE_ERROR = 0;
        constexpr uint8_t DEBUG = 1;
        constexpr uint8_t NMI = 2;
        constexpr uint8_t BREAKPOINT = 3;
        constexpr uint8_t OVERFLOW = 4;
        constexpr uint8_t BOUND_RANGE = 5;
        constexpr uint8_t INVALID_OPCODE = 6;
        constexpr uint8_t DEVICE_NOT_AVAIL = 7;
        constexpr uint8_t DOUBLE_FAULT = 8;
        constexpr uint8_t INVALID_TSS = 10;
        constexpr uint8_t SEGMENT_NOT_PRESENT = 11;
        constexpr uint8_t STACK_FAULT = 12;
        constexpr uint8_t GENERAL_PROTECTION = 13;
        constexpr uint8_t PAGE_FAULT = 14;
        constexpr uint8_t FPU_ERROR = 16;
        constexpr uint8_t ALIGNMENT_CHECK = 17;
        constexpr uint8_t MACHINE_CHECK = 18;
        constexpr uint8_t SIMD_ERROR = 19;

        constexpr uint8_t TIMER = 0x20;
        constexpr uint8_t KEYBOARD = 0x21;
        constexpr uint8_t PIC_SPURIOUS = 0x27;
        constexpr uint8_t MOUSE = 0x2C;

        constexpr uint8_t LAPIC_TIMER = 0x30;

        constexpr uint8_t SYSCALL = 0x80;
        constexpr uint8_t LAPIC_SPURIOUS = 0xFF;

        constexpr uint8_t DYNAMIC_BASE = 0x31;
        constexpr uint8_t DYNAMIC_MAX = 0xFE;
    }
}