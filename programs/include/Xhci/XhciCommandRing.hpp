#pragma once
#include <temrixstd.h>
#include "XhciCommon.hpp"
#include "XhciTransferRequestBlock.hpp"

class HostControllerCommandRing
{
public:
    static constexpr uint32_t TransferRequestBlockCount = 256;

    bool Initialize()
    {
        Syscall::Memory::DmaAllocResult dmaAllocationResult;
        uint64_t allocationStatus = Syscall::Memory::AllocDma(
            TransferRequestBlockCount * sizeof(TransferRequestBlock),
            XhciDeviceMemoryAllocationFlags,
            &dmaAllocationResult);
        if (allocationStatus != 0) return false;

        m_ringVirtualAddress = reinterpret_cast<TransferRequestBlock *>(dmaAllocationResult.virt);
        m_ringPhysicalAddress = dmaAllocationResult.phys;

        for (uint32_t index = 0; index < TransferRequestBlockCount; ++index)
            m_ringVirtualAddress[index] = {};

        TransferRequestBlock &linkTransferRequestBlock = m_ringVirtualAddress[TransferRequestBlockCount - 1];
        linkTransferRequestBlock.parameterLow  = (uint32_t)(m_ringPhysicalAddress & 0xFFFFFFFF);
        linkTransferRequestBlock.parameterHigh = (uint32_t)(m_ringPhysicalAddress >> 32);
        linkTransferRequestBlock.control = TransferRequestBlockControlWithType(TransferRequestBlockTypeLink) | (1u << 1);

        m_enqueueIndex = 0;
        m_cycleState = 1;

        return true;
    }

    uint64_t PhysicalBaseAddress() const { return m_ringPhysicalAddress; }

    uint64_t Enqueue(uint32_t parameterLow, uint32_t parameterHigh, uint32_t status, uint32_t transferRequestBlockType, uint32_t additionalControlBits = 0)
    {
        if (m_enqueueIndex == TransferRequestBlockCount - 1)
        {
            TransferRequestBlock &linkTransferRequestBlock = m_ringVirtualAddress[m_enqueueIndex];
            linkTransferRequestBlock.control = (linkTransferRequestBlock.control & ~0x1u) | (m_cycleState & 0x1);
            m_enqueueIndex = 0;
            m_cycleState ^= 1;
        }

        uint64_t transferRequestBlockPhysicalAddress =
            m_ringPhysicalAddress + m_enqueueIndex * sizeof(TransferRequestBlock);
        TransferRequestBlock &transferRequestBlock = m_ringVirtualAddress[m_enqueueIndex];

        transferRequestBlock.parameterLow  = parameterLow;
        transferRequestBlock.parameterHigh = parameterHigh;
        transferRequestBlock.status        = status;
        transferRequestBlock.control       = TransferRequestBlockControlWithType(transferRequestBlockType) | additionalControlBits | (m_cycleState & 0x1);

        m_enqueueIndex++;
        return transferRequestBlockPhysicalAddress;
    }

private:
    TransferRequestBlock *m_ringVirtualAddress = nullptr;
    uint64_t m_ringPhysicalAddress = 0;
    uint32_t m_enqueueIndex = 0;
    uint32_t m_cycleState = 1;
};