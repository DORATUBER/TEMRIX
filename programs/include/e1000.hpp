#pragma once
#include <temrixstd.h>

namespace E1000
{
    namespace Register
    {
        constexpr uint32_t Control              = 0x0000;
        constexpr uint32_t Status               = 0x0008;
        constexpr uint32_t EepromRead           = 0x0014;
        constexpr uint32_t InterruptCause       = 0x00C0;
        constexpr uint32_t InterruptMaskSet     = 0x00D0;
        constexpr uint32_t InterruptMaskClear   = 0x00D8;
        constexpr uint32_t RxDescBaseLow        = 0x2800;
        constexpr uint32_t RxDescBaseHigh       = 0x2804;
        constexpr uint32_t RxDescLength         = 0x2808;
        constexpr uint32_t RxDescHead           = 0x2810;
        constexpr uint32_t RxDescTail           = 0x2818;
        constexpr uint32_t ReceiveControl       = 0x0100;
        constexpr uint32_t TxDescBaseLow        = 0x3800;
        constexpr uint32_t TxDescBaseHigh       = 0x3804;
        constexpr uint32_t TxDescLength         = 0x3808;
        constexpr uint32_t TxDescHead           = 0x3810;
        constexpr uint32_t TxDescTail           = 0x3818;
        constexpr uint32_t TransmitControl      = 0x0400;
    }

    namespace ControlBit
    {
        constexpr uint32_t Reset              = (1 << 26);
        constexpr uint32_t SetLinkUp          = (1 << 6);
        constexpr uint32_t AutoSpeedDetect    = (1 << 5);
    }

    namespace ReceiveControlBit
    {
        constexpr uint32_t Enable             = (1 << 1);
        constexpr uint32_t AcceptBroadcast    = (1 << 15);
        constexpr uint32_t StripCRC           = (1 << 26);
    }

    namespace TransmitControlBit
    {
        constexpr uint32_t Enable             = (1 << 1);
        constexpr uint32_t PadShortPackets    = (1 << 3);
    }

    namespace TransmitCommand
    {
        constexpr uint8_t EndOfPacket         = (1 << 0);
        constexpr uint8_t InsertFrameChecksum = (1 << 1);
        constexpr uint8_t ReportStatus       = (1 << 3);
    }

    namespace TransmitStatus
    {
        constexpr uint8_t DescriptorDone     = (1 << 0);
    }

    namespace ReceiveStatus
    {
        constexpr uint8_t DescriptorDone     = (1 << 0);
        constexpr uint8_t EndOfPacket        = (1 << 1);
    }

    constexpr size_t   NumReceiveDescriptors  = 32;
    constexpr size_t   NumTransmitDescriptors = 8;
    constexpr uint32_t ReceiveBufferSize      = 2048;
    constexpr uint32_t TransmitBufferSize     = 2048;

    struct TransmitDescriptor
    {
        volatile uint64_t bufferAddress;
        volatile uint16_t length;
        volatile uint8_t  checksumOffset;
        volatile uint8_t  command;
        volatile uint8_t  status;
        volatile uint8_t  checksumStart;
        volatile uint16_t special;
    } __attribute__((packed));

    struct ReceiveDescriptor
    {
        volatile uint64_t bufferAddress;
        volatile uint16_t length;
        volatile uint16_t checksum;
        volatile uint8_t  status;
        volatile uint8_t  errors;
        volatile uint16_t special;
    } __attribute__((packed));

    class Controller
    {
    public:
        int init(uint64_t base)
        {
            mmioBase  = base;

            reset();
            disableInterrupts();
            readMacAddress();
            initReceive();
            initTransmit();

            return 0;
        }

        int send(void *data, uint16_t length)
        {
            if (length > TransmitBufferSize)
                return -1;  

            uint32_t tail = transmitTail;

            if (!(transmitDescriptors[tail].status & TransmitStatus::DescriptorDone))
                return -1;  

            
            
            
            Memory::Copy(transmitBuffers[tail], data, length);

            transmitDescriptors[tail].bufferAddress = transmitBuffersPhys[tail];
            transmitDescriptors[tail].length        = length;
            transmitDescriptors[tail].command       = TransmitCommand::EndOfPacket        |
                                                    TransmitCommand::InsertFrameChecksum |
                                                    TransmitCommand::ReportStatus;
            transmitDescriptors[tail].status        = 0;

            transmitTail = (tail + 1) % NumTransmitDescriptors;
            writeRegister(Register::TxDescTail, transmitTail);

            while (!(transmitDescriptors[tail].status & TransmitStatus::DescriptorDone));

            return 0;
        }

        int recv(void *buffer, uint16_t *length)
        {
            uint32_t           tail = receiveTail;
            ReceiveDescriptor *desc = &receiveDescriptors[tail];

            if (!(desc->status & ReceiveStatus::DescriptorDone))
                return -1;  

            *length = desc->length;
            Memory::Copy(buffer, receiveBuffers[tail], desc->length);

            desc->status = 0;
            writeRegister(Register::RxDescTail, tail);
            receiveTail = (tail + 1) % NumReceiveDescriptors;

            return 0;
        }

        uint8_t macAddress[6];

    private:
        uint64_t                 mmioBase;

        ReceiveDescriptor  *receiveDescriptors;
        uint64_t             receiveDescriptorsPhys;
        uint8_t            *receiveBuffers[NumReceiveDescriptors];
        uint64_t             receiveBuffersPhys[NumReceiveDescriptors];
        uint32_t              receiveTail;

        TransmitDescriptor *transmitDescriptors;
        uint64_t             transmitDescriptorsPhys;
        uint8_t             *transmitBuffers[NumTransmitDescriptors];
        uint64_t              transmitBuffersPhys[NumTransmitDescriptors];
        uint32_t              transmitTail;

        uint32_t readRegister(uint32_t reg)
        {
            return *(volatile uint32_t *)(mmioBase + reg);
        }

        void writeRegister(uint32_t reg, uint32_t value)
        {
            *(volatile uint32_t *)(mmioBase + reg) = value;
        }

        uint16_t readEeprom(uint8_t address)
        {
            writeRegister(Register::EepromRead, 1 | ((uint32_t)address << 8));
            uint32_t value;
            while (!((value = readRegister(Register::EepromRead)) & (1 << 4)));
            return (uint16_t)(value >> 16);
        }

        void readMacAddress()
        {
            uint16_t word0 = readEeprom(0);
            uint16_t word1 = readEeprom(1);
            uint16_t word2 = readEeprom(2);
            macAddress[0] = word0 & 0xFF;  macAddress[1] = word0 >> 8;
            macAddress[2] = word1 & 0xFF;  macAddress[3] = word1 >> 8;
            macAddress[4] = word2 & 0xFF;  macAddress[5] = word2 >> 8;
        }

        void reset()
        {
            uint32_t control = readRegister(Register::Control);
            writeRegister(Register::Control, control | ControlBit::Reset);
            while (readRegister(Register::Control) & ControlBit::Reset);

            control = readRegister(Register::Control);
            writeRegister(Register::Control,
                control | ControlBit::SetLinkUp | ControlBit::AutoSpeedDetect);
        }

        void disableInterrupts()
        {
            writeRegister(Register::InterruptMaskClear, 0xFFFFFFFF);
            readRegister(Register::InterruptCause);  
        }

        
        
        
        static void *allocDma(uint64_t size, uint64_t *physOut)
        {
            Syscall::Memory::DmaAllocResult result;
            Syscall::Memory::AllocDma(size,
                ::Memory::Read | ::Memory::Write | ::Memory::User,
                &result);
            *physOut = result.phys;
            return (void *)(uintptr_t)result.virt;
        }

        void initReceive()
        {
            uint64_t ringSize = (sizeof(ReceiveDescriptor) * NumReceiveDescriptors + 0xFFF) & ~0xFFFULL;
            receiveDescriptors = (ReceiveDescriptor *)allocDma(ringSize, &receiveDescriptorsPhys);

            for (size_t i = 0; i < NumReceiveDescriptors; i++) {
                uint64_t bufPhys;
                uint64_t bufSize = (ReceiveBufferSize + 0xFFF) & ~0xFFFULL;
                receiveBuffers[i]     = (uint8_t *)allocDma(bufSize, &bufPhys);
                receiveBuffersPhys[i] = bufPhys;

                receiveDescriptors[i].bufferAddress = bufPhys;
                receiveDescriptors[i].status        = 0;
            }

            writeRegister(Register::RxDescBaseLow,  (uint32_t)(receiveDescriptorsPhys & 0xFFFFFFFF));
            writeRegister(Register::RxDescBaseHigh, (uint32_t)(receiveDescriptorsPhys >> 32));
            writeRegister(Register::RxDescLength,   NumReceiveDescriptors * sizeof(ReceiveDescriptor));
            writeRegister(Register::RxDescHead,     0);
            writeRegister(Register::RxDescTail,     NumReceiveDescriptors - 1);
            receiveTail = 0;

            writeRegister(Register::ReceiveControl,
                ReceiveControlBit::Enable          |
                ReceiveControlBit::AcceptBroadcast |
                ReceiveControlBit::StripCRC);
        }

        void initTransmit()
        {
            uint64_t ringSize = (sizeof(TransmitDescriptor) * NumTransmitDescriptors + 0xFFF) & ~0xFFFULL;
            transmitDescriptors = (TransmitDescriptor *)allocDma(ringSize, &transmitDescriptorsPhys);

            for (size_t i = 0; i < NumTransmitDescriptors; i++) {
                uint64_t bufPhys;
                uint64_t bufSize = (TransmitBufferSize + 0xFFF) & ~0xFFFULL;
                transmitBuffers[i]     = (uint8_t *)allocDma(bufSize, &bufPhys);
                transmitBuffersPhys[i] = bufPhys;

                transmitDescriptors[i].bufferAddress = 0;
                transmitDescriptors[i].status        = TransmitStatus::DescriptorDone;
            }

            writeRegister(Register::TxDescBaseLow,  (uint32_t)(transmitDescriptorsPhys & 0xFFFFFFFF));
            writeRegister(Register::TxDescBaseHigh, (uint32_t)(transmitDescriptorsPhys >> 32));
            writeRegister(Register::TxDescLength,   NumTransmitDescriptors * sizeof(TransmitDescriptor));
            writeRegister(Register::TxDescHead,     0);
            writeRegister(Register::TxDescTail,     0);
            transmitTail = 0;

            writeRegister(Register::TransmitControl,
                TransmitControlBit::Enable          |
                TransmitControlBit::PadShortPackets |
                (0x0F << 4)                         |
                (0x3F << 12));
        }

    };
}