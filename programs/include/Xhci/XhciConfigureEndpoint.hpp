#pragma once
#include <temrixstd.h>
#include "XhciCommon.hpp"
#include "XhciTransferRequestBlock.hpp"
#include "XhciCommandRing.hpp"
#include "XhciDoorbell.hpp"
#include "XhciDeviceContext.hpp"
#include "XhciDeviceEnumeration.hpp"
#include "XhciEndpointTransfer.hpp"
#include "XhciHostController.hpp"

static inline uint32_t XhciConvertUsbIntervalToXhciInterval(uint8_t usbInterval, uint32_t portSpeed)
{
    bool isHighOrSuperSpeed = (portSpeed == 3 || portSpeed == 4 || portSpeed == 5);
    if (isHighOrSuperSpeed)
    {
        return usbInterval == 0 ? 0 : (uint32_t)(usbInterval - 1);
    }

    uint32_t frameInterval = usbInterval == 0 ? 1 : usbInterval;
    uint32_t exponent = 0;
    while ((1u << exponent) < frameInterval && exponent < 15) exponent++;
    return exponent + 3;
}

static inline bool XhciConfigureEndpoint(HostController &hostController, uint32_t slotId, uint32_t portNumberOneBased, uint32_t portSpeed,
                                          uint8_t endpointAddress, uint16_t maxPacketSize, uint8_t usbInterval,
                                          HostControllerCommandRing &endpointTransferRing)
{
    if (!endpointTransferRing.Initialize()) return false;

    uint32_t deviceContextIndex = XhciEndpointAddressToDeviceContextIndex(endpointAddress);
    const uint32_t contextSizeInBytes = hostController.ContextSizeInBytes();

    Syscall::Memory::DmaAllocResult inputContextAllocationResult;
    uint64_t inputContextAllocationStatus = Syscall::Memory::AllocDma(
        XhciInputContextEntryCount * contextSizeInBytes, XhciDeviceMemoryAllocationFlags, &inputContextAllocationResult);
    if (inputContextAllocationStatus != 0) return false;

    uint8_t *inputContextVirtualAddress = reinterpret_cast<uint8_t *>(inputContextAllocationResult.virt);
    for (uint32_t byteIndex = 0; byteIndex < XhciInputContextEntryCount * contextSizeInBytes; ++byteIndex)
        inputContextVirtualAddress[byteIndex] = 0;

    InputControlContext *inputControlContext = reinterpret_cast<InputControlContext *>(inputContextVirtualAddress);
    SlotContext *inputSlotContext = reinterpret_cast<SlotContext *>(inputContextVirtualAddress + contextSizeInBytes);
    EndpointContext *inputEndpointContext = reinterpret_cast<EndpointContext *>(inputContextVirtualAddress + (1 + deviceContextIndex) * contextSizeInBytes);

    inputControlContext->addContextFlags = (1u << 0) | (1u << deviceContextIndex);

    SlotContextSetRouteString(*inputSlotContext, 0);
    SlotContextSetSpeed(*inputSlotContext, portSpeed);
    SlotContextSetContextEntries(*inputSlotContext, deviceContextIndex);
    SlotContextSetRootHubPortNumber(*inputSlotContext, portNumberOneBased);

    bool isInterruptIn = UsbEndpointAddressIsIn(endpointAddress);
    EndpointContextSetErrorCount(*inputEndpointContext, 3);
    EndpointContextSetEndpointType(*inputEndpointContext, isInterruptIn ? EndpointTypeInterruptIn : EndpointTypeInterruptOut);
    EndpointContextSetMaxPacketSize(*inputEndpointContext, maxPacketSize);
    EndpointContextSetInterval(*inputEndpointContext, XhciConvertUsbIntervalToXhciInterval(usbInterval, portSpeed));
    EndpointContextSetTransferRingDequeuePointer(*inputEndpointContext, endpointTransferRing.PhysicalBaseAddress(), 1);
    EndpointContextSetAverageTransferRequestBlockLength(*inputEndpointContext, maxPacketSize);

    uint32_t slotIdControlBits = (slotId & 0xFF) << 24;
    uint64_t configureEndpointTransferRequestBlockPhysicalAddress = hostController.CommandRing().Enqueue(
        (uint32_t)(inputContextAllocationResult.phys & 0xFFFFFFFF),
        (uint32_t)(inputContextAllocationResult.phys >> 32),
        0,
        TransferRequestBlockTypeConfigureEndpointCommand,
        slotIdControlBits);

    XhciRingDoorbell(hostController.DoorbellRegisterSetBaseAddress(), 0);

    uint32_t completionCode = 0;
    uint32_t completionParameter = 0;
    if (!XhciWaitForCommandCompletion(hostController, configureEndpointTransferRequestBlockPhysicalAddress, completionCode, completionParameter))
    {
        String::Printf("[usb] configure endpoint command timed out, slot=%u\n", slotId);
        return false;
    }
    if (completionCode != CommandCompletionCodeSuccess)
    {
        String::Printf("[usb] configure endpoint command failed, slot=%u completion code=%u\n", slotId, completionCode);
        return false;
    }

    String::Printf("[usb] endpoint 0x%x configured, slot=%u dci=%u\n", endpointAddress, slotId, deviceContextIndex);
    return true;
}