#pragma once

#include "common.hpp"
#include "io.hpp"

namespace Hardware
{
    namespace MasterIRQ
    {
        constexpr uint8_t TIMER = (1 << 0);
        constexpr uint8_t KEYBOARD = (1 << 1);
        constexpr uint8_t CASCADE = (1 << 2);
        constexpr uint8_t COM2 = (1 << 3);
        constexpr uint8_t COM1 = (1 << 4);
        constexpr uint8_t LPT2 = (1 << 5);
        constexpr uint8_t FLOPPY = (1 << 6);
        constexpr uint8_t LPT1 = (1 << 7);
    }

    namespace SlaveIRQ
    {
        constexpr uint8_t RTC = (1 << 0);
        constexpr uint8_t ACPI = (1 << 1);
        constexpr uint8_t OPEN1 = (1 << 2);
        constexpr uint8_t OPEN2 = (1 << 3);
        constexpr uint8_t MOUSE = (1 << 4);
        constexpr uint8_t FPU = (1 << 5);
        constexpr uint8_t ATA1 = (1 << 6);
        constexpr uint8_t ATA2 = (1 << 7);
    }

    namespace PIC
    {
        constexpr uint16_t MASTER_COMMAND = 0x20;
        constexpr uint16_t MASTER_DATA = 0x21;
        constexpr uint16_t SLAVE_COMMAND = 0xA0;
        constexpr uint16_t SLAVE_DATA = 0xA1;

        constexpr uint8_t ICW1_INIT = 0x11;
        constexpr uint8_t ICW4_8086_MODE = 0x01;

        constexpr uint8_t MASTER_CASCADE_IRQ = 0x04;
        constexpr uint8_t SLAVE_CASCADE_ID = 0x02;

        constexpr uint8_t MASTER_VECTOR_OFFSET = 0x20;
        constexpr uint8_t SLAVE_VECTOR_OFFSET = 0x28;

        constexpr uint8_t EOI = 0x20;

        extern uint8_t g_masterMask;
        extern uint8_t g_slaveMask;

            inline void maskMaster(uint8_t irqs)
            {
                g_masterMask |= irqs;
                outb(MASTER_DATA, g_masterMask);
            }

            inline void maskSlave(uint8_t irqs)
            {
                g_slaveMask |= irqs;
                outb(SLAVE_DATA, g_slaveMask);
            }

        inline void unmaskMaster(uint8_t irqs)
        {
            g_masterMask &= ~irqs;
            outb(MASTER_DATA, g_masterMask);
        }

        inline void unmaskSlave(uint8_t irqs)
        {
            g_slaveMask &= ~irqs;
            outb(SLAVE_DATA, g_slaveMask);
        }

        inline void maskAll()
        {
            g_masterMask = 0xFF;
            g_slaveMask = 0xFF;
            outb(MASTER_DATA, g_masterMask);
            outb(SLAVE_DATA, g_slaveMask);
        }

        inline void sendEOI(bool fromSlave)
        {
            if (fromSlave)
                outb(SLAVE_COMMAND, EOI);
            outb(MASTER_COMMAND, EOI);
        }

        void init();
    }
}