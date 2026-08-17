#pragma once
#include <temrixstd.h>
#include "XhciCommon.hpp"
#include "XhciTransferRequestBlock.hpp"
#include "XhciCommandRing.hpp"
#include "XhciDoorbell.hpp"
#include "XhciDeviceContext.hpp"
#include "XhciHostController.hpp"

static inline bool XhciWaitForCommandCompletion(HostController &hostController, uint64_t commandTransferRequestBlockPhysicalAddress, uint32_t &outCompletionCode, uint32_t &outCompletionParameter)
{
    for (uint32_t iteration = 0; iteration < 1000000; ++iteration)
    {
        TransferRequestBlock event;
        if (!hostController.EventRing().TryDequeue(event)) continue;
        if (TransferRequestBlockGetType(event) != TransferRequestBlockTypeCommandCompletionEvent) continue;

        uint64_t eventSourceTransferRequestBlockPhysicalAddress =
            ((uint64_t)event.parameterHigh << 32) | event.parameterLow;
        if (eventSourceTransferRequestBlockPhysicalAddress != commandTransferRequestBlockPhysicalAddress) continue;

        outCompletionCode = (event.status >> 24) & 0xFF;
        outCompletionParameter = (event.control >> 24) & 0xFF;
        return true;
    }
    return false;
}

static inline uint32_t XhciEnableSlot(HostController &hostController)
{
    uint64_t enableSlotTransferRequestBlockPhysicalAddress =
        hostController.CommandRing().Enqueue(0, 0, 0, TransferRequestBlockTypeEnableSlotCommand);
    XhciRingDoorbell(hostController.DoorbellRegisterSetBaseAddress(), 0);

    uint32_t completionCode = 0;
    uint32_t slotId = 0;
    if (!XhciWaitForCommandCompletion(hostController, enableSlotTransferRequestBlockPhysicalAddress, completionCode, slotId))
    {
        String::Printf("[usb] enable slot command timed out\n");
        return 0;
    }
    if (completionCode != CommandCompletionCodeSuccess)
    {
        String::Printf("[usb] enable slot command failed, completion code=%u\n", completionCode);
        return 0;
    }

    return slotId;
}

static inline bool XhciAddressDevice(HostController &hostController, uint32_t slotId, uint32_t portNumberOneBased, uint32_t portSpeed, HostControllerCommandRing &controlEndpointTransferRing)
{
    const uint32_t contextSizeInBytes = hostController.ContextSizeInBytes();

    Syscall::Memory::DmaAllocResult inputContextAllocationResult;
    uint64_t inputContextAllocationStatus = Syscall::Memory::AllocDma(
        XhciInputContextEntryCount * contextSizeInBytes,
        XhciDeviceMemoryAllocationFlags,
        &inputContextAllocationResult);
    if (inputContextAllocationStatus != 0) return false;

    Syscall::Memory::DmaAllocResult outputDeviceContextAllocationResult;
    uint64_t outputDeviceContextAllocationStatus = Syscall::Memory::AllocDma(
        XhciDeviceContextEntryCount * contextSizeInBytes,
        XhciDeviceMemoryAllocationFlags,
        &outputDeviceContextAllocationResult);
    if (outputDeviceContextAllocationStatus != 0) return false;

    uint8_t *inputContextVirtualAddress = reinterpret_cast<uint8_t *>(inputContextAllocationResult.virt);
    for (uint32_t byteIndex = 0; byteIndex < XhciInputContextEntryCount * contextSizeInBytes; ++byteIndex)
        inputContextVirtualAddress[byteIndex] = 0;

    uint8_t *outputDeviceContextVirtualAddress = reinterpret_cast<uint8_t *>(outputDeviceContextAllocationResult.virt);
    for (uint32_t byteIndex = 0; byteIndex < XhciDeviceContextEntryCount * contextSizeInBytes; ++byteIndex)
        outputDeviceContextVirtualAddress[byteIndex] = 0;

    InputControlContext *inputControlContext = reinterpret_cast<InputControlContext *>(inputContextVirtualAddress);
    SlotContext *inputSlotContext = reinterpret_cast<SlotContext *>(inputContextVirtualAddress + contextSizeInBytes);
    EndpointContext *inputControlEndpointContext = reinterpret_cast<EndpointContext *>(inputContextVirtualAddress + 2 * contextSizeInBytes);

    inputControlContext->addContextFlags = (1u << 0) | (1u << 1);

    SlotContextSetRouteString(*inputSlotContext, 0);
    SlotContextSetSpeed(*inputSlotContext, portSpeed);
    SlotContextSetContextEntries(*inputSlotContext, 1);
    SlotContextSetRootHubPortNumber(*inputSlotContext, portNumberOneBased);

    uint32_t maxPacketSize = XhciMaxPacketSizeForPortSpeed(portSpeed);
    EndpointContextSetErrorCount(*inputControlEndpointContext, 3);
    EndpointContextSetEndpointType(*inputControlEndpointContext, EndpointTypeControl);
    EndpointContextSetMaxPacketSize(*inputControlEndpointContext, maxPacketSize);
    EndpointContextSetTransferRingDequeuePointer(*inputControlEndpointContext, controlEndpointTransferRing.PhysicalBaseAddress(), 1);
    EndpointContextSetAverageTransferRequestBlockLength(*inputControlEndpointContext, 8);

    hostController.DeviceContextBaseAddressArray().SetSlotDeviceContextPhysicalAddress(slotId, outputDeviceContextAllocationResult.phys);

    uint32_t addressDeviceSlotIdControlBits = (slotId & 0xFF) << 24;
    uint64_t addressDeviceTransferRequestBlockPhysicalAddress = hostController.CommandRing().Enqueue(
        (uint32_t)(inputContextAllocationResult.phys & 0xFFFFFFFF),
        (uint32_t)(inputContextAllocationResult.phys >> 32),
        0,
        TransferRequestBlockTypeAddressDeviceCommand,
        addressDeviceSlotIdControlBits);

    XhciRingDoorbell(hostController.DoorbellRegisterSetBaseAddress(), 0);

    uint32_t completionCode = 0;
    uint32_t completionParameter = 0;
    if (!XhciWaitForCommandCompletion(hostController, addressDeviceTransferRequestBlockPhysicalAddress, completionCode, completionParameter))
    {
        String::Printf("[usb] address device command timed out\n");
        return false;
    }
    if (completionCode != CommandCompletionCodeSuccess)
    {
        String::Printf("[usb] address device command failed, completion code=%u\n", completionCode);
        return false;
    }

    String::Printf("[usb] device addressed, slot=%u port=%u speed=%u\n", slotId, portNumberOneBased, portSpeed);
    return true;
}