#pragma once
#include <temrixstd.h>
#include "XhciCommon.hpp"

class HostControllerDeviceContextBaseAddressArray
{
public:
    static constexpr uint32_t MaximumSlotCount = 256;

    bool Initialize()
    {
        Syscall::Memory::DmaAllocResult allocationResult;
        uint64_t allocationStatus = Syscall::Memory::AllocDma(
            MaximumSlotCount * sizeof(uint64_t),
            XhciDeviceMemoryAllocationFlags,
            &allocationResult);
        if (allocationStatus != 0) return false;

        m_arrayVirtualAddress = reinterpret_cast<uint64_t *>(allocationResult.virt);
        m_arrayPhysicalAddress = allocationResult.phys;

        for (uint32_t index = 0; index < MaximumSlotCount; ++index)
            m_arrayVirtualAddress[index] = 0;

        return true;
    }

    uint64_t PhysicalBaseAddress() const { return m_arrayPhysicalAddress; }

    void SetSlotDeviceContextPhysicalAddress(uint32_t slotId, uint64_t deviceContextPhysicalAddress)
    {
        m_arrayVirtualAddress[slotId] = deviceContextPhysicalAddress;
    }

private:
    uint64_t *m_arrayVirtualAddress = nullptr;
    uint64_t m_arrayPhysicalAddress = 0;
};