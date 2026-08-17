#include <temrixstd.h>
#include "Xhci/XhciHostController.hpp"
#include "Xhci/XhciPortStatus.hpp"
#include "Xhci/XhciDeviceEnumeration.hpp"
#include "Xhci/XhciControlTransfer.hpp"
#include "Xhci/XhciConfigurationDescriptor.hpp"
#include "Xhci/XhciHidPolling.hpp"
#include "Xhci/HidBootProtocol.hpp"
#include "InputServer.hpp"

int main(int argc, char **argv)
{
    String::Print("[usb] starting\n");

    Syscall::Pci::KernelDevice devices[64];
    uint64_t count = Syscall::Pci::GetDevices(devices, 64);

    int foundIndex = -1;
    for (uint64_t i = 0; i < count; i++)
    {
        if (devices[i].classCode == 0x0C && devices[i].subclass == 0x03 /* && devices[i].progIf == 0x30 */)
        {
            foundIndex = (int)i;
            break;
        }
    }

    if (foundIndex < 0)
    {
        String::Print("[usb] no xHCI controller found\n");
        Syscall::IO::Flush();
        return -1;
    }

    uint64_t barVirtualBaseAddress = Syscall::Memory::MapBar(foundIndex, 0);
    if (!barVirtualBaseAddress)
    {
        String::Printf("[usb] failed to map xHCI BAR0 \n");
        return -1;
    }

    HostController hostController;
    if (!hostController.Initialize(barVirtualBaseAddress))
    {
        String::Print("[usb] xHCI host controller init failed\n");
        return -1;
    }

    Syscall::Interrupt::VectorAllocResult vecAlloc;
    if (Syscall::Interrupt::AllocVectors(1, &vecAlloc) != 0)
    {
        String::Print("[usb] failed to allocate interrupt vector\n");
        return -1;
    }
    else{
        String::Print("[usb] Succeded to allocate interrupt vector\n");
    }
    uint8_t xhciVector = vecAlloc.base0;

    Syscall::Interrupt::Subscribe(xhciVector);

    if (Syscall::Pci::MsixEnable(foundIndex, 0, xhciVector) != 0)
    {
        String::Print("[usb] failed to enable MSI-X\n");
        return -1;
    }
    else{
        String::Print("[usb] Succeded to enable MSI-X\n");
    }

    hostController.ClearPendingInterrupt();

    String::Print("[usb] xHCI host controller running, ready for device enumeration\n");

    InputServer inputServer;
    if (!inputServer.init())
    {
        String::Print("[usb] failed to init input server\n");
        return -1;
    }

    HidDeviceManager hidDevices;

    for (uint32_t portNumberOneBased = 1; portNumberOneBased <= hostController.MaxPorts(); ++portNumberOneBased)
    {
        uint32_t portStatusAndControlValue = XhciReadPortStatusAndControl(hostController.OperationalRegisterSetBaseAddress(), portNumberOneBased);
        if (!XhciPortIsConnected(portStatusAndControlValue)) continue;

        String::Printf("[usb] device connected on port %u\n", portNumberOneBased);
        XhciPortClearConnectStatusChange(hostController.OperationalRegisterSetBaseAddress(), portNumberOneBased);

        if (!XhciPortResetAndWaitForEnabled(hostController.OperationalRegisterSetBaseAddress(), portNumberOneBased))
        {
            String::Printf("[usb] port %u reset failed or did not enable\n", portNumberOneBased);
            continue;
        }

        portStatusAndControlValue = XhciReadPortStatusAndControl(hostController.OperationalRegisterSetBaseAddress(), portNumberOneBased);
        uint32_t portSpeed = XhciPortGetSpeed(portStatusAndControlValue);

        uint32_t slotId = XhciEnableSlot(hostController);
        if (slotId == 0)
        {
            String::Printf("[usb] enable slot failed for port %u\n", portNumberOneBased);
            continue;
        }

        HostControllerCommandRing controlEndpointTransferRing;
        if (!controlEndpointTransferRing.Initialize())
        {
            String::Printf("[usb] failed to allocate control endpoint transfer ring for slot %u\n", slotId);
            continue;
        }

        if (!XhciAddressDevice(hostController, slotId, portNumberOneBased, portSpeed, controlEndpointTransferRing))
        {
            String::Printf("[usb] address device failed for slot %u\n", slotId);
            continue;
        }

        UsbDeviceDescriptor deviceDescriptor;
        if (!XhciGetDeviceDescriptor(hostController, slotId, controlEndpointTransferRing, deviceDescriptor))
        {
            String::Printf("[usb] get device descriptor failed for slot %u\n", slotId);
            continue;
        }

        ParsedUsbConfiguration parsedConfiguration;
        if (!XhciGetConfigurationDescriptor(hostController, slotId, controlEndpointTransferRing, parsedConfiguration))
        {
            String::Printf("[usb] get configuration descriptor failed for slot %u\n", slotId);
            continue;
        }

        UsbEndpointDescriptor keyboardEndpoint;
        if (HidDeviceManager::FindHidBootEndpoint(parsedConfiguration, UsbHidProtocolKeyboard, keyboardEndpoint))
        {
            hidDevices.SetUpDevice(hostController, slotId, portNumberOneBased, portSpeed,
                                    keyboardEndpoint, /*isKeyboard=*/true, HidBootKeyboardReportSize);
            continue;
        }

        UsbEndpointDescriptor mouseEndpoint;
        if (HidDeviceManager::FindHidBootEndpoint(parsedConfiguration, UsbHidProtocolMouse, mouseEndpoint))
        {
            hidDevices.SetUpDevice(hostController, slotId, portNumberOneBased, portSpeed,
                                    mouseEndpoint, /*isKeyboard=*/false, 3);
        }
    }

    if (hidDevices.DeviceCount() == 0)
    {
        String::Print("[usb] no HID boot devices found, exiting\n");
        Syscall::IO::Flush();
        return -1;
    }

    String::Printf("[usb] polling %llu HID device(s)\n", hidDevices.DeviceCount());

    Syscall::IO::Flush();

    for (;;)
    {
        hidDevices.Poll(hostController,
            [&](const HidBootKeyboardReport &report) {
                inputServer.PushKeyboardEvent(report.modifierKeys, report.keyCodes);
            },
            [&](const HidBootMouseReport &report) {
                inputServer.PushMouseEvent(report.buttonState, report.deltaX, report.deltaY);
            });

        Syscall::Interrupt::Subscribe(xhciVector); 
        Syscall::Process::Wait();
    }
    return 0;
}