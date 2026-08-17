#pragma once
#include <temrixstd.h>
#include "XhciCommon.hpp"
#include "XhciTransferRequestBlock.hpp"
#include "XhciCommandRing.hpp"
#include "XhciDoorbell.hpp"
#include "XhciHostController.hpp"

static constexpr uint32_t UsbControlEndpointDeviceContextIndex = 1;

static constexpr uint8_t UsbRequestTypeDeviceToHostStandardDevice = 0x80;
static constexpr uint8_t UsbRequestGetDescriptor = 0x06;
static constexpr uint8_t UsbDescriptorTypeDevice = 0x01;

static constexpr uint32_t TransferStageDirectionOut = 0;
static constexpr uint32_t TransferStageDirectionIn  = 1;

static constexpr uint32_t TransferTypeNoDataStage  = 0;
static constexpr uint32_t TransferTypeOutDataStage = 2;
static constexpr uint32_t TransferTypeInDataStage  = 3;

struct __attribute__((packed)) UsbDeviceDescriptor
{
    uint8_t  length;
    uint8_t  descriptorType;
    uint16_t usbVersion;
    uint8_t  deviceClass;
    uint8_t  deviceSubClass;
    uint8_t  deviceProtocol;
    uint8_t  maxPacketSize0;
    uint16_t vendorId;
    uint16_t productId;
    uint16_t deviceVersion;
    uint8_t  manufacturerStringIndex;
    uint8_t  productStringIndex;
    uint8_t  serialNumberStringIndex;
    uint8_t  numConfigurations;
};
static_assert(sizeof(UsbDeviceDescriptor) == 18, "UsbDeviceDescriptor must be 18 bytes");

static inline bool XhciWaitForTransferCompletion(HostController &hostController, uint64_t statusStageTransferRequestBlockPhysicalAddress, uint32_t &outCompletionCode)
{
    for (uint32_t iteration = 0; iteration < 1000000; ++iteration)
    {
        TransferRequestBlock event;
        if (!hostController.EventRing().TryDequeue(event)) continue;
        if (TransferRequestBlockGetType(event) != TransferRequestBlockTypeTransferEvent) continue;

        uint64_t eventSourceTransferRequestBlockPhysicalAddress =
            ((uint64_t)event.parameterHigh << 32) | event.parameterLow;
        if (eventSourceTransferRequestBlockPhysicalAddress != statusStageTransferRequestBlockPhysicalAddress) continue;

        outCompletionCode = (event.status >> 24) & 0xFF;
        return true;
    }
    return false;
}

static inline bool XhciControlTransferIn(HostController &hostController, uint32_t slotId, HostControllerCommandRing &controlEndpointTransferRing,
                                          uint8_t requestType, uint8_t request, uint16_t value, uint16_t index, uint16_t length,
                                          uint64_t dataBufferPhysicalAddress)
{
    uint32_t setupPacketParameterLow  = requestType | ((uint32_t)request << 8) | ((uint32_t)value << 16);
    uint32_t setupPacketParameterHigh = index | ((uint32_t)length << 16);
    uint32_t setupStageStatus = 8;
    uint32_t setupStageControlBits = (1u << 6) /* Immediate Data */ | (TransferTypeInDataStage << 16) /* TRT = IN Data Stage */;
    controlEndpointTransferRing.Enqueue(setupPacketParameterLow, setupPacketParameterHigh, setupStageStatus, TransferRequestBlockTypeSetupStage, setupStageControlBits);

    uint32_t dataStageStatus = length;
    uint32_t dataStageControlBits = (TransferStageDirectionIn << 16);
    controlEndpointTransferRing.Enqueue(
        (uint32_t)(dataBufferPhysicalAddress & 0xFFFFFFFF),
        (uint32_t)(dataBufferPhysicalAddress >> 32),
        dataStageStatus, TransferRequestBlockTypeDataStage, dataStageControlBits);

    uint32_t statusStageControlBits = (TransferStageDirectionOut << 16) | (1u << 5) /* Interrupt On Completion */;
    uint64_t statusStageTransferRequestBlockPhysicalAddress =
        controlEndpointTransferRing.Enqueue(0, 0, 0, TransferRequestBlockTypeStatusStage, statusStageControlBits);

    XhciRingDoorbell(hostController.DoorbellRegisterSetBaseAddress(), slotId, UsbControlEndpointDeviceContextIndex);

    uint32_t completionCode = 0;
    if (!XhciWaitForTransferCompletion(hostController, statusStageTransferRequestBlockPhysicalAddress, completionCode))
    {
        String::Printf("[usb] control transfer timed out, slot=%u\n", slotId);
        return false;
    }
    if (completionCode != CommandCompletionCodeSuccess && completionCode != CommandCompletionCodeShortPacket)
    {
        String::Printf("[usb] control transfer failed, slot=%u completion code=%u\n", slotId, completionCode);
        return false;
    }

    return true;
}

static inline bool XhciGetDeviceDescriptor(HostController &hostController, uint32_t slotId, HostControllerCommandRing &controlEndpointTransferRing, UsbDeviceDescriptor &outDeviceDescriptor)
{
    Syscall::Memory::DmaAllocResult descriptorBufferAllocationResult;
    uint64_t descriptorBufferAllocationStatus = Syscall::Memory::AllocDma(
        sizeof(UsbDeviceDescriptor), XhciDeviceMemoryAllocationFlags, &descriptorBufferAllocationResult);
    if (descriptorBufferAllocationStatus != 0) return false;

    uint16_t descriptorValue = ((uint16_t)UsbDescriptorTypeDevice << 8) | 0;
    bool transferSucceeded = XhciControlTransferIn(
        hostController, slotId, controlEndpointTransferRing,
        UsbRequestTypeDeviceToHostStandardDevice, UsbRequestGetDescriptor, descriptorValue, 0, sizeof(UsbDeviceDescriptor),
        descriptorBufferAllocationResult.phys);
    if (!transferSucceeded) return false;

    uint8_t *descriptorBufferVirtualAddress = reinterpret_cast<uint8_t *>(descriptorBufferAllocationResult.virt);
    ::Memory::Copy(&outDeviceDescriptor, descriptorBufferVirtualAddress, sizeof(UsbDeviceDescriptor));

    String::Printf("[usb] device descriptor: vendor=0x%x product=0x%x class=%u maxPacketSize0=%u numConfigurations=%u\n",
                    outDeviceDescriptor.vendorId, outDeviceDescriptor.productId, outDeviceDescriptor.deviceClass,
                    outDeviceDescriptor.maxPacketSize0, outDeviceDescriptor.numConfigurations);

    return true;
}