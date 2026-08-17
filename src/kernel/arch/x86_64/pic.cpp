#include "pic.hpp"

namespace Hardware
{
    uint8_t PIC::g_masterMask = 0xFF;
    uint8_t PIC::g_slaveMask = 0xFF;

    namespace PIC
    {
        void init()
        {
            outb(MASTER_COMMAND, ICW1_INIT);
            outb(SLAVE_COMMAND, ICW1_INIT);

            outb(MASTER_DATA, MASTER_VECTOR_OFFSET);
            outb(SLAVE_DATA, SLAVE_VECTOR_OFFSET);

            outb(MASTER_DATA, MASTER_CASCADE_IRQ);
            outb(SLAVE_DATA, SLAVE_CASCADE_ID);

            outb(MASTER_DATA, ICW4_8086_MODE);
            outb(SLAVE_DATA, ICW4_8086_MODE);

            maskAll();
        }
    }
}