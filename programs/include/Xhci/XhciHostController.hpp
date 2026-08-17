#pragma once
#include <temrixstd.h>
#include "XhciCommon.hpp"
#include "XhciTransferRequestBlock.hpp"
#include "XhciCommandRing.hpp"
#include "XhciEventRing.hpp"
#include "XhciDeviceContextBaseAddressArray.hpp"

static constexpr uint64_t XhciCapabilityRegisterHostControllerStructuralParameters1Offset = 0x04;
static constexpr uint64_t XhciCapabilityRegisterHostControllerCapabilityParameters1Offset = 0x10;
static constexpr uint64_t XhciCapabilityRegisterDoorbellOffsetOffset = 0x14;
static constexpr uint64_t XhciCapabilityRegisterRuntimeRegisterSpaceOffsetOffset = 0x18;

static constexpr uint64_t XhciOperationalUsbCommandRegisterOffset = 0x00;
static constexpr uint64_t XhciOperationalUsbStatusRegisterOffset = 0x04;
static constexpr uint64_t XhciOperationalPageSizeRegisterOffset = 0x08;
static constexpr uint64_t XhciOperationalDeviceNotificationControlOffset = 0x14;
static constexpr uint64_t XhciOperationalCommandRingControlRegisterOffset = 0x18;
static constexpr uint64_t XhciOperationalDeviceContextBaseAddressArrayPointerOffset = 0x30;
static constexpr uint64_t XhciOperationalConfigureRegisterOffset = 0x38;

static constexpr uint32_t UsbCommandRegisterRunStopBit = 1u << 0;
static constexpr uint32_t UsbCommandRegisterHostControllerResetBit = 1u << 1;
static constexpr uint32_t UsbCommandRegisterInterrupterEnableBit = 1u << 2;

static constexpr uint32_t UsbStatusRegisterHostControllerHaltedBit = 1u << 0;
static constexpr uint32_t UsbStatusRegisterControllerNotReadyBit = 1u << 11;

static constexpr uint64_t XhciRuntimeMicroframeIndexRegisterOffset = 0x00;
static constexpr uint64_t XhciRuntimeFirstInterrupterOffset = 0x20;

static inline uint32_t XhciExtractMaxSlots(uint32_t structuralParameters1) { return structuralParameters1 & 0xFF; }
static inline uint32_t XhciExtractMaxPorts(uint32_t structuralParameters1) { return (structuralParameters1 >> 24) & 0xFF; }
static inline bool XhciExtractContextSize64(uint32_t capabilityParameters1) { return (capabilityParameters1 & (1u << 2)) != 0; }

class HostController
{
public:
    bool Initialize(uint64_t barVirtualBaseAddress)
    {
        uint64_t capabilityRegisterSetBaseAddress = barVirtualBaseAddress;

        uint32_t capabilityRegisterDwordZero = XhciReadRegister32(capabilityRegisterSetBaseAddress, 0x00);
        uint32_t capabilityRegistersLength = capabilityRegisterDwordZero & 0xFF;
        m_hostControllerInterfaceVersion = (capabilityRegisterDwordZero >> 16) & 0xFFFF;

        uint32_t structuralParameters1 = XhciReadRegister32(capabilityRegisterSetBaseAddress, XhciCapabilityRegisterHostControllerStructuralParameters1Offset);
        uint32_t capabilityParameters1 = XhciReadRegister32(capabilityRegisterSetBaseAddress, XhciCapabilityRegisterHostControllerCapabilityParameters1Offset);
        uint32_t doorbellOffset = XhciReadRegister32(capabilityRegisterSetBaseAddress, XhciCapabilityRegisterDoorbellOffsetOffset);
        uint32_t runtimeRegisterSpaceOffset = XhciReadRegister32(capabilityRegisterSetBaseAddress, XhciCapabilityRegisterRuntimeRegisterSpaceOffsetOffset);

        m_maxSlots = XhciExtractMaxSlots(structuralParameters1);
        m_maxPorts = XhciExtractMaxPorts(structuralParameters1);
        m_is64BitAddressingCapable = (capabilityParameters1 & 0x1) != 0;
        m_contextSizeInBytes = XhciExtractContextSize64(capabilityParameters1) ? 64 : 32;

        m_operationalRegisterSetBaseAddress = capabilityRegisterSetBaseAddress + capabilityRegistersLength;
        m_runtimeRegisterSetBaseAddress = capabilityRegisterSetBaseAddress + (runtimeRegisterSpaceOffset & ~0x1Fu);
        m_doorbellRegisterSetBaseAddress = capabilityRegisterSetBaseAddress + (doorbellOffset & ~0x3u);
        m_interrupterZeroRegisterSetBaseAddress = m_runtimeRegisterSetBaseAddress + XhciRuntimeFirstInterrupterOffset;

        String::Printf("[usb] xHCI: version=0x%x slots=%u ports=%u%s (%uB contexts)\n",
                        m_hostControllerInterfaceVersion, m_maxSlots, m_maxPorts,
                        m_is64BitAddressingCapable ? " (64-bit capable)" : " (32-bit only)",
                        m_contextSizeInBytes);

        if (!ResetController())
            return false;
        if (!m_deviceContextBaseAddressArray.Initialize())
            return false;
        if (!m_commandRing.Initialize())
            return false;
        if (!m_eventRing.Initialize(m_interrupterZeroRegisterSetBaseAddress))
            return false;

        XhciWriteRegister32(m_operationalRegisterSetBaseAddress, XhciOperationalConfigureRegisterOffset, m_maxSlots);
        XhciWriteRegister64(m_operationalRegisterSetBaseAddress, XhciOperationalDeviceContextBaseAddressArrayPointerOffset, m_deviceContextBaseAddressArray.PhysicalBaseAddress());
        XhciWriteRegister64(m_operationalRegisterSetBaseAddress, XhciOperationalCommandRingControlRegisterOffset, m_commandRing.PhysicalBaseAddress() | 0x1);

        uint32_t interrupterManagementValue = XhciReadRegister32(m_interrupterZeroRegisterSetBaseAddress, XhciInterrupterManagementRegisterOffset);
        XhciWriteRegister32(m_interrupterZeroRegisterSetBaseAddress, XhciInterrupterManagementRegisterOffset, interrupterManagementValue | InterrupterManagementRegisterInterruptEnableBit);

        uint32_t commandRegisterValue = XhciReadRegister32(m_operationalRegisterSetBaseAddress, XhciOperationalUsbCommandRegisterOffset);
        commandRegisterValue |= UsbCommandRegisterRunStopBit | UsbCommandRegisterInterrupterEnableBit;
        XhciWriteRegister32(m_operationalRegisterSetBaseAddress, XhciOperationalUsbCommandRegisterOffset, commandRegisterValue);

        for (uint32_t iteration = 0; iteration < 100000; ++iteration)
        {
            uint32_t statusRegisterValue = XhciReadRegister32(m_operationalRegisterSetBaseAddress, XhciOperationalUsbStatusRegisterOffset);
            if ((statusRegisterValue & UsbStatusRegisterHostControllerHaltedBit) == 0)
            {
                String::Printf("[usb] xHCI host controller running\n");
                return true;
            }
        }

        String::Printf("[usb] xHCI host controller failed to leave halted state\n");
        return false;
    }

    void ClearPendingInterrupt()
    {
        uint32_t value = XhciReadRegister32(m_interrupterZeroRegisterSetBaseAddress, XhciInterrupterManagementRegisterOffset);
        value |= InterrupterManagementRegisterInterruptPendingBit;
        XhciWriteRegister32(m_interrupterZeroRegisterSetBaseAddress, XhciInterrupterManagementRegisterOffset, value);
    }

    uint32_t MaxSlots() const { return m_maxSlots; }
    uint32_t MaxPorts() const { return m_maxPorts; }
    bool     Is64BitAddressingCapable() const { return m_is64BitAddressingCapable; }
    uint32_t ContextSizeInBytes() const { return m_contextSizeInBytes; }
    uint64_t OperationalRegisterSetBaseAddress() const { return m_operationalRegisterSetBaseAddress; }
    uint64_t RuntimeRegisterSetBaseAddress() const { return m_runtimeRegisterSetBaseAddress; }
    uint64_t DoorbellRegisterSetBaseAddress() const { return m_doorbellRegisterSetBaseAddress; }

    HostControllerCommandRing &CommandRing() { return m_commandRing; }
    HostControllerEventRing &EventRing() { return m_eventRing; }
    HostControllerDeviceContextBaseAddressArray &DeviceContextBaseAddressArray() { return m_deviceContextBaseAddressArray; }

private:
    bool ResetController()
    {
        uint32_t commandRegisterValue = XhciReadRegister32(m_operationalRegisterSetBaseAddress, XhciOperationalUsbCommandRegisterOffset);
        commandRegisterValue &= ~UsbCommandRegisterRunStopBit;
        XhciWriteRegister32(m_operationalRegisterSetBaseAddress, XhciOperationalUsbCommandRegisterOffset, commandRegisterValue);

        for (uint32_t iteration = 0; iteration < 100000; ++iteration)
        {
            uint32_t statusRegisterValue = XhciReadRegister32(m_operationalRegisterSetBaseAddress, XhciOperationalUsbStatusRegisterOffset);
            if (statusRegisterValue & UsbStatusRegisterHostControllerHaltedBit)
                break;
        }

        commandRegisterValue = XhciReadRegister32(m_operationalRegisterSetBaseAddress, XhciOperationalUsbCommandRegisterOffset);
        XhciWriteRegister32(m_operationalRegisterSetBaseAddress, XhciOperationalUsbCommandRegisterOffset, commandRegisterValue | UsbCommandRegisterHostControllerResetBit);

        for (uint32_t iteration = 0; iteration < 100000; ++iteration)
        {
            uint32_t commandRegisterValueDuringReset = XhciReadRegister32(m_operationalRegisterSetBaseAddress, XhciOperationalUsbCommandRegisterOffset);
            uint32_t statusRegisterValueDuringReset = XhciReadRegister32(m_operationalRegisterSetBaseAddress, XhciOperationalUsbStatusRegisterOffset);
            bool resetBitCleared = (commandRegisterValueDuringReset & UsbCommandRegisterHostControllerResetBit) == 0;
            bool controllerReady = (statusRegisterValueDuringReset & UsbStatusRegisterControllerNotReadyBit) == 0;
            if (resetBitCleared && controllerReady)
            {
                String::Printf("[usb] xHCI host controller reset complete\n");
                return true;
            }
        }

        String::Printf("[usb] xHCI host controller reset timed out\n");
        return false;
    }

    uint64_t m_operationalRegisterSetBaseAddress = 0;
    uint64_t m_runtimeRegisterSetBaseAddress = 0;
    uint64_t m_doorbellRegisterSetBaseAddress = 0;
    uint64_t m_interrupterZeroRegisterSetBaseAddress = 0;

    uint32_t m_hostControllerInterfaceVersion = 0;
    uint32_t m_maxSlots = 0;
    uint32_t m_maxPorts = 0;
    bool     m_is64BitAddressingCapable = false;
    uint32_t m_contextSizeInBytes = 32;

    HostControllerCommandRing m_commandRing;
    HostControllerEventRing m_eventRing;
    HostControllerDeviceContextBaseAddressArray m_deviceContextBaseAddressArray;
};