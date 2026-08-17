#include "idt.hpp"
#include "isr.hpp"

namespace Hardware
{
    namespace IDT
    {
        __attribute__((aligned(0x10))) static Entry interruptDescriptorTable[ENTRY_COUNT];
        static Register interruptDescriptorTableRegister;

        void setEntry(uint8_t vector, void *handler, uint8_t attributes, uint8_t ist)
        {
            Entry &e = interruptDescriptorTable[vector];
            uint64_t addr = (uint64_t)handler;

            e.isr_low = addr & 0xFFFF;
            e.kernel_cs = KERNEL_CS;
            e.ist = ist;
            e.attributes = attributes;
            e.isr_mid = (addr >> 16) & 0xFFFF;
            e.isr_high = (addr >> 32) & 0xFFFFFFFF;
            e.reserved = 0;
        }

        void load()
        {
            interruptDescriptorTableRegister.limit = sizeof(interruptDescriptorTable) - 1;
            interruptDescriptorTableRegister.base = (uint64_t)&interruptDescriptorTable[0];
            loadInterruptDescriptorTableRegister(interruptDescriptorTableRegister);
        }

        void loadOnCurrentCPU() {
            loadInterruptDescriptorTableRegister(interruptDescriptorTableRegister);
        }

        void init()
        {
            for (int i = 0; i < ENTRY_COUNT; i++)
                setEntry(i, (void *)Hardware::ISR::defaultISR);
        }
    }
}