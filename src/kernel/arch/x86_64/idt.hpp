#pragma once

#include "common.hpp"

namespace Hardware
{
    namespace IDT
    {
        constexpr uint16_t KERNEL_CS = 0x08;
        constexpr uint8_t IST_NONE = 0;
        constexpr int ENTRY_COUNT = 256;
        constexpr uint8_t PRESENT = 0x80;

        enum class Ring : uint8_t
        {
            Kernel = 0x00,
            User = 0x60
        };

        enum class GateType : uint8_t
        {
            Interrupt = 0x0E,
            Trap = 0x0F
        };

        constexpr uint8_t makeGate(Ring ring, GateType type)
        {
            return PRESENT | (uint8_t)ring | (uint8_t)type;
        }

        constexpr uint8_t KERNEL_INTERRUPT = makeGate(Ring::Kernel, GateType::Interrupt);
        constexpr uint8_t USER_INTERRUPT = makeGate(Ring::User, GateType::Interrupt);
        constexpr uint8_t KERNEL_TRAP = makeGate(Ring::Kernel, GateType::Trap);
        constexpr uint8_t USER_TRAP = makeGate(Ring::User, GateType::Trap);

        struct __attribute__((packed)) Entry
        {
            uint16_t isr_low;   
            uint16_t kernel_cs; 
            uint8_t ist;        
            uint8_t attributes; 
            uint16_t isr_mid;   
            uint32_t isr_high;  
            uint32_t reserved;
        };

        struct __attribute__((packed)) Register
        {
            uint16_t limit;
            uint64_t base;
        };

        inline void loadInterruptDescriptorTableRegister(Register interruptDescriptorTableRegister){
            asm volatile("lidt %0" : : "m"(interruptDescriptorTableRegister));
        }

        void setEntry(uint8_t vector, void *handler,
                      uint8_t attributes = KERNEL_INTERRUPT,
                      uint8_t ist = IST_NONE);
        void load();
        void init();

        void loadOnCurrentCPU();
    }
}