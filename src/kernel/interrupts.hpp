#pragma once
#include "common.hpp"
#include "io.hpp"
#include "kernel/MemoryCommon.hpp"
#include "kernel/BuddyAllocator.hpp"
#include "kernel/PageTable.hpp"
#include "gdt.hpp"
#include "pit.hpp"
#include "lapic.hpp"
#include "vector.hpp"
#include "tss.hpp"
#include "pic.hpp"
#include "idt.hpp"
#include "isr.hpp"

struct Task;

extern "C" void timerStub();

extern "C" void exceptionStub0();
extern "C" void exceptionStub1();
extern "C" void exceptionStub3();
extern "C" void exceptionStub4();
extern "C" void exceptionStub5();
extern "C" void exceptionStub6();
extern "C" void exceptionStub7();
extern "C" void exceptionStub16();
extern "C" void exceptionStub19();

extern "C" void exceptionErrorStub10();
extern "C" void exceptionErrorStub11();
extern "C" void exceptionErrorStub12();
extern "C" void exceptionErrorStub13();
extern "C" void exceptionErrorStub17();

extern "C" void pagefault_isr();

namespace Hardware{
    void initLapicTimer();
    void rearmLapicTimerIfNeeded();
}

namespace Hardware
{
    namespace PS2
    {
        constexpr uint16_t DATA_PORT = 0x60;
        constexpr uint16_t STATUS_PORT = 0x64;

        constexpr uint8_t STATUS_OUTPUT_FULL = 0x01;
        constexpr uint8_t MOUSE_BUTTON_MASK = 0x07;
        constexpr uint8_t MOUSE_X_SIGN = 0x10;
        constexpr uint8_t MOUSE_Y_SIGN = 0x20;
        constexpr uint8_t MOUSE_OVERFLOW_MASK = 0xC0;
        constexpr uint8_t MOUSE_SYNC_BIT = 0x08;

        inline void drainBuffer()
        {
            while (inb(STATUS_PORT) & STATUS_OUTPUT_FULL)
                inb(DATA_PORT);
        }
    }

    struct VectorRange
    {
        uint8_t base;
        uint8_t count;
        VectorRange *next;
    };

    struct VectorAllocation
    {
        VectorRange ranges[2];
        uint8_t rangeCount; 
    };

    struct IrqWaiter
    {
        uint8_t vector;
        Task *task;
        bool enabled;
        bool used;
    };

    constexpr uint32_t MAX_IRQ_WAITERS = 64;
    extern IrqWaiter g_irqWaiters[MAX_IRQ_WAITERS];
    uint64_t irqDispatch(uint8_t vector, uint64_t rsp);

    class InterruptController
    {
    public:
        InterruptController() = default;

        void registerMasterIRQ(uint8_t vector, uint8_t irq, void *handler,
                               uint8_t attributes = IDT::KERNEL_INTERRUPT)
        {
            IDT::setEntry(vector, handler, attributes);
            PIC::unmaskMaster(irq);
        }

        void registerSlaveIRQ(uint8_t vector, uint8_t irq, void *handler,
                              uint8_t attributes = IDT::KERNEL_INTERRUPT)
        {
            IDT::setEntry(vector, handler, attributes);
            PIC::unmaskMaster(MasterIRQ::CASCADE);
            PIC::unmaskSlave(irq);
        }

        void registerMSIX(uint8_t vector, void *handler,
                          uint8_t attributes = IDT::KERNEL_INTERRUPT)
        {
            IDT::setEntry(vector, handler, attributes);
        }

        void registerHandler(uint8_t vector, void *handler,
                             uint8_t attributes = IDT::KERNEL_INTERRUPT)
        {
            IDT::setEntry(vector, handler, attributes);
        }

        static void eoi(bool fromSlave = false)
        {
            PIC::sendEOI(fromSlave);
        }

        void init();
        void registerExceptions();

        VectorAllocation allocVectors(uint32_t count);
        void freeVectors(VectorAllocation alloc);

    private:
        VectorRange *m_freeList = nullptr;
        void mergeAdjacent();
    };
}