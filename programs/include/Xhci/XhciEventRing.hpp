#pragma once
#include <temrixstd.h>
#include "XhciCommon.hpp"
#include "XhciTransferRequestBlock.hpp"

static constexpr uint64_t XhciInterrupterManagementRegisterOffset = 0x00;
static constexpr uint64_t XhciInterrupterModerationRegisterOffset = 0x04;
static constexpr uint64_t XhciInterrupterEventRingSegmentTableSizeRegisterOffset = 0x08;
static constexpr uint64_t XhciInterrupterEventRingSegmentTableBaseAddressRegisterOffset = 0x10;
static constexpr uint64_t XhciInterrupterEventRingDequeuePointerRegisterOffset = 0x18;

static constexpr uint32_t InterrupterManagementRegisterInterruptPendingBit = 1u << 0;
static constexpr uint32_t InterrupterManagementRegisterInterruptEnableBit = 1u << 1;

struct EventRingSegmentTableEntry
{
    uint64_t ringSegmentBaseAddress;
    uint32_t ringSegmentSize;
    uint32_t reservedField;
};
static_assert(sizeof(EventRingSegmentTableEntry) == 16, "EventRingSegmentTableEntry must be 16 bytes");

class HostControllerEventRing
{
public:
    static constexpr uint32_t TransferRequestBlockCount = 256;

    bool Initialize(uint64_t interrupterRegisterSetBaseAddress)
    {
        m_interrupterRegisterSetBaseAddress = interrupterRegisterSetBaseAddress;

        Syscall::Memory::DmaAllocResult ringAllocationResult;
        uint64_t ringAllocationStatus = Syscall::Memory::AllocDma(
            TransferRequestBlockCount * sizeof(TransferRequestBlock),
            XhciDeviceMemoryAllocationFlags,
            &ringAllocationResult);
        if (ringAllocationStatus != 0)
            return false;

        Syscall::Memory::DmaAllocResult eventRingSegmentTableAllocationResult;
        uint64_t eventRingSegmentTableAllocationStatus = Syscall::Memory::AllocDma(
            sizeof(EventRingSegmentTableEntry),
            XhciDeviceMemoryAllocationFlags,
            &eventRingSegmentTableAllocationResult);
        if (eventRingSegmentTableAllocationStatus != 0)
            return false;

        m_ringVirtualAddress = reinterpret_cast<TransferRequestBlock *>(ringAllocationResult.virt);
        m_ringPhysicalAddress = ringAllocationResult.phys;

        for (uint32_t index = 0; index < TransferRequestBlockCount; ++index)
            m_ringVirtualAddress[index] = {};

        EventRingSegmentTableEntry *eventRingSegmentTableVirtualAddress =
            reinterpret_cast<EventRingSegmentTableEntry *>(eventRingSegmentTableAllocationResult.virt);
        m_eventRingSegmentTablePhysicalAddress = eventRingSegmentTableAllocationResult.phys;

        eventRingSegmentTableVirtualAddress[0].ringSegmentBaseAddress = m_ringPhysicalAddress;
        eventRingSegmentTableVirtualAddress[0].ringSegmentSize = TransferRequestBlockCount;
        eventRingSegmentTableVirtualAddress[0].reservedField = 0;

        m_dequeueIndex = 0;
        m_cycleState = 1;

        XhciWriteRegister32(m_interrupterRegisterSetBaseAddress, XhciInterrupterEventRingSegmentTableSizeRegisterOffset, 1);
        XhciWriteRegister64(m_interrupterRegisterSetBaseAddress, XhciInterrupterEventRingDequeuePointerRegisterOffset, m_ringPhysicalAddress);
        XhciWriteRegister64(m_interrupterRegisterSetBaseAddress, XhciInterrupterEventRingSegmentTableBaseAddressRegisterOffset, m_eventRingSegmentTablePhysicalAddress);

        return true;
    }

    bool TryDequeue(TransferRequestBlock &outputTransferRequestBlock)
    {
        TransferRequestBlock &candidateTransferRequestBlock = m_ringVirtualAddress[m_dequeueIndex];
        if (TransferRequestBlockGetCycleBit(candidateTransferRequestBlock) != (bool)m_cycleState)
            return false;

        outputTransferRequestBlock = candidateTransferRequestBlock;

        m_dequeueIndex++;
        if (m_dequeueIndex == TransferRequestBlockCount)
        {
            m_dequeueIndex = 0;
            m_cycleState ^= 1;
        }

        uint64_t dequeuePointerPhysicalAddress =
            m_ringPhysicalAddress + m_dequeueIndex * sizeof(TransferRequestBlock);
        XhciWriteRegister64(m_interrupterRegisterSetBaseAddress, XhciInterrupterEventRingDequeuePointerRegisterOffset, dequeuePointerPhysicalAddress | 0x8);

        return true;
    }

private:
    uint64_t m_interrupterRegisterSetBaseAddress = 0;
    TransferRequestBlock *m_ringVirtualAddress = nullptr;
    uint64_t m_ringPhysicalAddress = 0;
    uint64_t m_eventRingSegmentTablePhysicalAddress = 0;
    uint32_t m_dequeueIndex = 0;
    uint32_t m_cycleState = 1;
};