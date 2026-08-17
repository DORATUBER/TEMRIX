#pragma once
#include <temrixstd.h>
#include "XhciHostController.hpp"
#include "XhciConfigureEndpoint.hpp"
#include "XhciEndpointTransfer.hpp"
#include "XhciConfigurationDescriptor.hpp"
#include "HidBootProtocol.hpp"

static constexpr uint8_t UsbClassHid = 0x03;
static constexpr uint8_t UsbHidSubclassBoot = 0x01;
static constexpr uint8_t UsbHidProtocolKeyboard = 0x01;
static constexpr uint8_t UsbHidProtocolMouse = 0x02;

static constexpr uint64_t MaxHidDevices = 8;

struct HidPollState
{
    bool active = false;
    uint32_t slotId = 0;
    uint32_t deviceContextIndex = 0;
    bool isKeyboard = false; 
    HostControllerCommandRing interruptTransferRing;
    uint64_t reportBufferPhysical = 0;
    uint8_t *reportBufferVirtual = nullptr;
    uint32_t reportSize = 0;
    bool transferPending = false;
    uint64_t pendingTrbPhysicalAddress = 0;

    uint8_t previousReportBytes[8] = {};
    bool hasPreviousReport = false;
};

class HidDeviceManager
{
public:
    static bool FindHidBootEndpoint(const ParsedUsbConfiguration &parsedConfiguration, uint8_t wantProtocol,
                                    UsbEndpointDescriptor &outEndpointDescriptor)
    {
        for (uint32_t interfaceIndex = 0; interfaceIndex < parsedConfiguration.interfaceCount; ++interfaceIndex)
        {
            const ParsedUsbInterface &parsedInterface = parsedConfiguration.interfaces[interfaceIndex];
            const UsbInterfaceDescriptor &ifaceDesc = parsedInterface.interfaceDescriptor;

            if (ifaceDesc.interfaceClass != UsbClassHid)
                continue;
            if (ifaceDesc.interfaceSubClass != UsbHidSubclassBoot)
                continue;
            if (ifaceDesc.interfaceProtocol != wantProtocol)
                continue;

            for (uint32_t endpointIndex = 0; endpointIndex < parsedInterface.endpointCount; ++endpointIndex)
            {
                const UsbEndpointDescriptor &endpointDescriptor = parsedInterface.endpointDescriptors[endpointIndex];
                if (UsbEndpointAddressIsIn(endpointDescriptor.endpointAddress) &&
                    UsbEndpointGetTransferType(endpointDescriptor) == UsbEndpointTransferTypeInterrupt)
                {
                    outEndpointDescriptor = endpointDescriptor;
                    return true;
                }
            }
        }
        return false;
    }

    bool SetUpDevice(HostController &hostController, uint32_t slotId, uint32_t portNumberOneBased,
                     uint32_t portSpeed, const UsbEndpointDescriptor &endpointDescriptor,
                     bool isKeyboard, uint32_t reportSize)
    {
        if (m_deviceCount >= MaxHidDevices)
        {
            String::Print("[usb] too many HID devices, skipping\n");
            return false;
        }

        HidPollState &deviceState = m_devices[m_deviceCount];
        deviceState = {};

        if (!XhciConfigureEndpoint(hostController, slotId, portNumberOneBased, portSpeed,
                                   endpointDescriptor.endpointAddress, endpointDescriptor.maxPacketSize,
                                   endpointDescriptor.interval, deviceState.interruptTransferRing))
        {
            String::Printf("[usb] configure endpoint failed for slot=%u endpoint=0x%x\n",
                           slotId, endpointDescriptor.endpointAddress);
            return false;
        }

        Syscall::Memory::DmaAllocResult reportBufferAllocationResult;
        uint64_t reportBufferAllocationStatus = Syscall::Memory::AllocDma(
            reportSize, XhciDeviceMemoryAllocationFlags, &reportBufferAllocationResult);
        if (reportBufferAllocationStatus != 0)
        {
            String::Print("[usb] failed to allocate HID report buffer\n");
            return false;
        }

        deviceState.active = true;
        deviceState.slotId = slotId;
        deviceState.deviceContextIndex = XhciEndpointAddressToDeviceContextIndex(endpointDescriptor.endpointAddress);
        deviceState.isKeyboard = isKeyboard;
        deviceState.reportBufferPhysical = reportBufferAllocationResult.phys;
        deviceState.reportBufferVirtual = reinterpret_cast<uint8_t *>(reportBufferAllocationResult.virt);
        deviceState.reportSize = reportSize;

        SubmitTransfer(hostController, deviceState);

        m_deviceCount++;
        String::Printf("[usb] %s ready on slot=%u endpoint=0x%x dci=%u\n",
                       isKeyboard ? "keyboard" : "mouse", slotId,
                       endpointDescriptor.endpointAddress, deviceState.deviceContextIndex);
        return true;
    }

    uint64_t DeviceCount() const { return m_deviceCount; }

    template <typename OnKeyboardReport, typename OnMouseReport>
    void Poll(HostController &hostController, OnKeyboardReport &&onKeyboardReport, OnMouseReport &&onMouseReport)
    {
        TransferRequestBlock event;
        while (XhciPollAnyEvent(hostController, event))
        {
            uint32_t eventSlotId = XhciTransferEventGetSlotId(event);
            uint32_t eventEndpointId = XhciTransferEventGetEndpointId(event);
            uint32_t completionCode = XhciTransferEventGetCompletionCode(event);

            HidPollState *matchedDevice = FindByEndpoint(eventSlotId, eventEndpointId);
            if (matchedDevice == nullptr)
                continue;

            matchedDevice->transferPending = false;

            if (completionCode != CommandCompletionCodeSuccess)
            {
                SubmitTransfer(hostController, *matchedDevice);
                continue;
            }

            uint32_t residualLength = XhciTransferEventGetResidualLength(event);
            uint32_t bytesTransferred = matchedDevice->reportSize > residualLength
                                            ? matchedDevice->reportSize - residualLength
                                            : 0;

            HandleReport(*matchedDevice, bytesTransferred, onKeyboardReport, onMouseReport);
            SubmitTransfer(hostController, *matchedDevice);
        }
    }

private:
    HidPollState m_devices[MaxHidDevices];
    uint64_t m_deviceCount = 0;

    HidPollState *FindByEndpoint(uint32_t slotId, uint32_t deviceContextIndex)
    {
        for (uint64_t deviceIndex = 0; deviceIndex < m_deviceCount; ++deviceIndex)
        {
            HidPollState &candidate = m_devices[deviceIndex];
            if (candidate.active && candidate.slotId == slotId &&
                candidate.deviceContextIndex == deviceContextIndex)
                return &candidate;
        }
        return nullptr;
    }

    static void SubmitTransfer(HostController &hostController, HidPollState &deviceState)
    {
        deviceState.pendingTrbPhysicalAddress = XhciSubmitNormalTransfer(
            deviceState.interruptTransferRing, deviceState.reportBufferPhysical, deviceState.reportSize, true);
        XhciRingEndpointDoorbell(hostController, deviceState.slotId, deviceState.deviceContextIndex);
        deviceState.transferPending = true;
    }

    template <typename OnKeyboardReport, typename OnMouseReport>
    static void HandleReport(HidPollState &deviceState, uint32_t bytesTransferred,
                             OnKeyboardReport &&onKeyboardReport, OnMouseReport &&onMouseReport)
    {
        uint32_t minimumRequiredSize = deviceState.isKeyboard ? HidBootKeyboardReportSize : 3;
        if (bytesTransferred < minimumRequiredSize)
            return;

        bool isUnchanged = deviceState.hasPreviousReport &&
                           (Memory::Compare(deviceState.previousReportBytes, deviceState.reportBufferVirtual, minimumRequiredSize) == 0);

        ::Memory::Copy(deviceState.previousReportBytes, deviceState.reportBufferVirtual, minimumRequiredSize);
        deviceState.hasPreviousReport = true;

        if (isUnchanged)
            return;

        if (deviceState.isKeyboard)
        {
            HidBootKeyboardReport keyboardReport;
            HidDecodeBootKeyboardReport(deviceState.reportBufferVirtual, keyboardReport);
            onKeyboardReport(keyboardReport);
        }
        else
        {
            HidBootMouseReport mouseReport;
            HidDecodeBootMouseReport(deviceState.reportBufferVirtual, mouseReport);
            onMouseReport(mouseReport);
        }
    }
};