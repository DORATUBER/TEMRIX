#pragma once
#include <temrixstd.h>
#include "XhciCommon.hpp"
#include "XhciCommandRing.hpp"
#include "XhciControlTransfer.hpp"
#include "XhciHostController.hpp"

static constexpr uint8_t UsbDescriptorTypeConfiguration = 0x02;
static constexpr uint8_t UsbDescriptorTypeInterface      = 0x04;
static constexpr uint8_t UsbDescriptorTypeEndpoint       = 0x05;

static constexpr uint32_t UsbConfigurationDescriptorMaxTotalLength = 4096;
static constexpr uint32_t UsbMaxInterfaceCount = 8;
static constexpr uint32_t UsbMaxEndpointCountPerInterface = 8;

struct __attribute__((packed)) UsbConfigurationDescriptor
{
    uint8_t  length;
    uint8_t  descriptorType;
    uint16_t totalLength;
    uint8_t  numInterfaces;
    uint8_t  configurationValue;
    uint8_t  configurationStringIndex;
    uint8_t  attributes;
    uint8_t  maxPower;
};
static_assert(sizeof(UsbConfigurationDescriptor) == 9, "UsbConfigurationDescriptor must be 9 bytes");

struct __attribute__((packed)) UsbInterfaceDescriptor
{
    uint8_t length;
    uint8_t descriptorType;
    uint8_t interfaceNumber;
    uint8_t alternateSetting;
    uint8_t numEndpoints;
    uint8_t interfaceClass;
    uint8_t interfaceSubClass;
    uint8_t interfaceProtocol;
    uint8_t interfaceStringIndex;
};
static_assert(sizeof(UsbInterfaceDescriptor) == 9, "UsbInterfaceDescriptor must be 9 bytes");

struct __attribute__((packed)) UsbEndpointDescriptor
{
    uint8_t  length;
    uint8_t  descriptorType;
    uint8_t  endpointAddress;
    uint8_t  attributes;
    uint16_t maxPacketSize;
    uint8_t  interval;
};
static_assert(sizeof(UsbEndpointDescriptor) == 7, "UsbEndpointDescriptor must be 7 bytes");

static inline bool UsbEndpointAddressIsIn(uint8_t endpointAddress) { return (endpointAddress & 0x80) != 0; }
static inline uint8_t UsbEndpointAddressGetNumber(uint8_t endpointAddress) { return endpointAddress & 0x0F; }

enum UsbEndpointTransferType : uint8_t
{
    UsbEndpointTransferTypeControl     = 0,
    UsbEndpointTransferTypeIsochronous = 1,
    UsbEndpointTransferTypeBulk        = 2,
    UsbEndpointTransferTypeInterrupt   = 3,
};

static inline uint8_t UsbEndpointGetTransferType(const UsbEndpointDescriptor &endpointDescriptor)
{
    return endpointDescriptor.attributes & 0x3;
}

struct ParsedUsbInterface
{
    UsbInterfaceDescriptor interfaceDescriptor;
    UsbEndpointDescriptor endpointDescriptors[UsbMaxEndpointCountPerInterface];
    uint32_t endpointCount;
};

struct ParsedUsbConfiguration
{
    UsbConfigurationDescriptor configurationDescriptor;
    ParsedUsbInterface interfaces[UsbMaxInterfaceCount];
    uint32_t interfaceCount;
};

static inline bool XhciGetConfigurationDescriptor(HostController &hostController, uint32_t slotId, HostControllerCommandRing &controlEndpointTransferRing, ParsedUsbConfiguration &outParsedConfiguration)
{
    Syscall::Memory::DmaAllocResult descriptorBufferAllocationResult;
    uint64_t descriptorBufferAllocationStatus = Syscall::Memory::AllocDma(
        UsbConfigurationDescriptorMaxTotalLength, XhciDeviceMemoryAllocationFlags, &descriptorBufferAllocationResult);
    if (descriptorBufferAllocationStatus != 0)
    {
        String::Printf("[usb] config descriptor: DMA alloc failed, status=%llu\n", descriptorBufferAllocationStatus);
        return false;
    }

    uint16_t descriptorValue = ((uint16_t)UsbDescriptorTypeConfiguration << 8) | 0;

    bool headerFetchSucceeded = XhciControlTransferIn(
        hostController, slotId, controlEndpointTransferRing,
        UsbRequestTypeDeviceToHostStandardDevice, UsbRequestGetDescriptor, descriptorValue, 0, sizeof(UsbConfigurationDescriptor),
        descriptorBufferAllocationResult.phys);
    if (!headerFetchSucceeded)
    {
        String::Printf("[usb] config descriptor: header fetch (9 bytes) failed\n");
        return false;
    }

    uint8_t *descriptorBufferVirtualAddress = reinterpret_cast<uint8_t *>(descriptorBufferAllocationResult.virt);

    UsbConfigurationDescriptor headerOnlyConfigurationDescriptor;
    ::Memory::Copy(&headerOnlyConfigurationDescriptor, descriptorBufferVirtualAddress, sizeof(UsbConfigurationDescriptor));

    uint32_t totalLength = headerOnlyConfigurationDescriptor.totalLength;
    String::Printf("[usb] config descriptor: header ok, length=%u type=%u totalLength=%u numInterfaces=%u\n",
                    headerOnlyConfigurationDescriptor.length,
                    headerOnlyConfigurationDescriptor.descriptorType,
                    totalLength,
                    headerOnlyConfigurationDescriptor.numInterfaces);

    if (totalLength == 0 || totalLength > UsbConfigurationDescriptorMaxTotalLength)
    {
        String::Printf("[usb] config descriptor: totalLength out of range (%u), max=%u\n",
                        totalLength, UsbConfigurationDescriptorMaxTotalLength);
        return false;
    }

    bool fullFetchSucceeded = XhciControlTransferIn(
        hostController, slotId, controlEndpointTransferRing,
        UsbRequestTypeDeviceToHostStandardDevice, UsbRequestGetDescriptor, descriptorValue, 0, (uint16_t)totalLength,
        descriptorBufferAllocationResult.phys);
    if (!fullFetchSucceeded)
    {
        String::Printf("[usb] config descriptor: full fetch (%u bytes) failed\n", totalLength);
        return false;
    }

    String::Printf("[usb] config descriptor: full fetch ok\n");

    outParsedConfiguration = {};
    ::Memory::Copy(&outParsedConfiguration.configurationDescriptor, descriptorBufferVirtualAddress, sizeof(UsbConfigurationDescriptor));

    uint32_t byteOffset = sizeof(UsbConfigurationDescriptor);
    ParsedUsbInterface *currentInterface = nullptr;

    while (byteOffset < totalLength && outParsedConfiguration.interfaceCount < UsbMaxInterfaceCount)
    {
        uint8_t descriptorLength = descriptorBufferVirtualAddress[byteOffset];
        uint8_t descriptorType = descriptorBufferVirtualAddress[byteOffset + 1];
        if (descriptorLength == 0)
        {
            String::Printf("[usb] config descriptor: zero-length sub-descriptor at offset %u, stopping parse\n", byteOffset);
            break;
        }

        if (descriptorType == UsbDescriptorTypeInterface && descriptorLength == sizeof(UsbInterfaceDescriptor))
        {
            currentInterface = &outParsedConfiguration.interfaces[outParsedConfiguration.interfaceCount];
            ::Memory::Copy(&currentInterface->interfaceDescriptor, descriptorBufferVirtualAddress + byteOffset, sizeof(UsbInterfaceDescriptor));
            currentInterface->endpointCount = 0;
            outParsedConfiguration.interfaceCount++;
        }
        else if (descriptorType == UsbDescriptorTypeEndpoint && descriptorLength == sizeof(UsbEndpointDescriptor) && currentInterface != nullptr)
        {
            if (currentInterface->endpointCount < UsbMaxEndpointCountPerInterface)
            {
                ::Memory::Copy(&currentInterface->endpointDescriptors[currentInterface->endpointCount], descriptorBufferVirtualAddress + byteOffset, sizeof(UsbEndpointDescriptor));
                currentInterface->endpointCount++;
            }
        }

        byteOffset += descriptorLength;
    }

    String::Printf("[usb] config descriptor: parsed %u interface(s)\n", outParsedConfiguration.interfaceCount);

    for (uint32_t interfaceIndex = 0; interfaceIndex < outParsedConfiguration.interfaceCount; ++interfaceIndex)
    {
        ParsedUsbInterface &parsedInterface = outParsedConfiguration.interfaces[interfaceIndex];
        String::Printf("[usb] interface %u: class=%u subClass=%u protocol=%u endpoints=%u\n",
                        parsedInterface.interfaceDescriptor.interfaceNumber,
                        parsedInterface.interfaceDescriptor.interfaceClass,
                        parsedInterface.interfaceDescriptor.interfaceSubClass,
                        parsedInterface.interfaceDescriptor.interfaceProtocol,
                        parsedInterface.endpointCount);

        for (uint32_t endpointIndex = 0; endpointIndex < parsedInterface.endpointCount; ++endpointIndex)
        {
            UsbEndpointDescriptor &endpointDescriptor = parsedInterface.endpointDescriptors[endpointIndex];
            String::Printf("[usb]   endpoint address=0x%x %s type=%u maxPacketSize=%u\n",
                            endpointDescriptor.endpointAddress,
                            UsbEndpointAddressIsIn(endpointDescriptor.endpointAddress) ? "IN" : "OUT",
                            UsbEndpointGetTransferType(endpointDescriptor),
                            endpointDescriptor.maxPacketSize);
        }
    }

    return true;
}