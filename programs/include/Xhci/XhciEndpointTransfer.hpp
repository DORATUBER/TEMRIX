#pragma once
#include <temrixstd.h>
#include "XhciCommon.hpp"
#include "XhciTransferRequestBlock.hpp"
#include "XhciCommandRing.hpp"
#include "XhciDoorbell.hpp"
#include "XhciHostController.hpp"
#include "XhciConfigurationDescriptor.hpp"

static inline uint32_t XhciEndpointAddressToDeviceContextIndex(uint8_t endpointAddress)
{
    uint32_t endpointNumber = UsbEndpointAddressGetNumber(endpointAddress);
    uint32_t isIn = UsbEndpointAddressIsIn(endpointAddress) ? 1 : 0;
    return endpointNumber == 0 ? 1 : (2 * endpointNumber + isIn);
}

static inline uint64_t XhciSubmitNormalTransfer(HostControllerCommandRing &endpointTransferRing, uint64_t bufferPhysicalAddress, uint32_t transferLength, bool interruptOnCompletion)
{
    uint32_t controlBits = interruptOnCompletion ? (1u << 5) : 0;
    return endpointTransferRing.Enqueue(
        (uint32_t)(bufferPhysicalAddress & 0xFFFFFFFF),
        (uint32_t)(bufferPhysicalAddress >> 32),
        transferLength & 0x1FFFF,
        TransferRequestBlockTypeNormal,
        controlBits);
}

static inline void XhciRingEndpointDoorbell(HostController &hostController, uint32_t slotId, uint32_t deviceContextIndex)
{
    XhciRingDoorbell(hostController.DoorbellRegisterSetBaseAddress(), slotId, deviceContextIndex);
}

static inline bool XhciPollAnyEvent(HostController &hostController, TransferRequestBlock &outEvent)
{
    return hostController.EventRing().TryDequeue(outEvent);
}

static inline uint32_t XhciTransferEventGetSlotId(const TransferRequestBlock &event)
{
    return (event.control >> 24) & 0xFF;
}

static inline uint32_t XhciTransferEventGetEndpointId(const TransferRequestBlock &event)
{
    return (event.control >> 16) & 0x1F;
}

static inline uint32_t XhciTransferEventGetCompletionCode(const TransferRequestBlock &event)
{
    return (event.status >> 24) & 0xFF;
}

static inline uint32_t XhciTransferEventGetResidualLength(const TransferRequestBlock &event)
{
    return event.status & 0xFFFFFF;
}

static inline uint64_t XhciTransferEventGetSourceTransferRequestBlockPhysicalAddress(const TransferRequestBlock &event)
{
    return ((uint64_t)event.parameterHigh << 32) | event.parameterLow;
}