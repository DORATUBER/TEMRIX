#include "isr.hpp"
#include "pic.hpp"
#include "lapic.hpp"
#include "kernel/MemoryCommon.hpp"
#include "kernel/BuddyAllocator.hpp"
#include "kernel/PageTable.hpp"
#include "timer.hpp"
#include "KernelState.hpp"
#include "Serial.hpp"

extern Kernel kernel;

namespace Hardware{
    __attribute__((interrupt)) void ISR::defaultISR(void* frame)
    {
        outb(PIC::MASTER_COMMAND, 0x0B);
        uint8_t isr = inb(PIC::MASTER_COMMAND);
        if (isr) PIC::sendEOI(false);
    }

    __attribute__((interrupt)) void ISR::keyboard(void* frame)
    {
        uint8_t scancode = inb(0x60);
        if (kernel.sharedDataRO && kernel.sharedDataRW) {
            uint8_t head = kernel.sharedDataRO->kbHead;
            uint8_t next = (head + 1) & 0xFF;
            if (next != kernel.sharedDataRW->kbTail) {
                kernel.sharedDataRO->kbBuf[head] = scancode;
                kernel.sharedDataRO->kbHead = next;
            }
        }
        PIC::sendEOI(false);
    }
    
    __attribute__((interrupt)) void ISR::lapicSpuriousISR(void* frame) {}

    __attribute__((interrupt)) void ISR::lapicTimer(void* frame)
    {
        Hardware::g_ticks+=1;
        LAPIC::eoi();
    }

    __attribute__((interrupt)) void ISR::timer(void* frame)
    {
        Hardware::g_ticks+=1;
        PIC::sendEOI(false);
    }

    static void kernelPanic(const char* msg, bool hasCode = false, uintptr_t code = 0)
    {
        asm volatile("cli");

        Serial::print("\n*** KERNEL PANIC ***\n");
        Serial::print(msg);
        if (hasCode)
            Serial::printf(" [err=0x%llx]", code);
        Serial::print("\n*** SYSTEM HALTED ***\n");

        asm volatile("hlt");
    }

    __attribute__((interrupt)) void ISR::nmi(void* frame) { kernelPanic("*** NMI: Non-Maskable Interrupt ***"); }
    __attribute__((interrupt)) void ISR::machineCheck(void* frame) { kernelPanic("*** #MC: Machine Check ***"); }
    __attribute__((interrupt)) void ISR::doubleFault(void* frame, uintptr_t code) 
    {
        const char* msg = "\n*** KERNEL PANIC ***\n*** #DF: Double Fault ***\n*** SYSTEM HALTED ***\n";
        while (*msg) {
            while (!(inb(0x3FD) & 0x20));
            outb(0x3F8, *msg++);
        }
        asm volatile("cli; hlt");
    }
}