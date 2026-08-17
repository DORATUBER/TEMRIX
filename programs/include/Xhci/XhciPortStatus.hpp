#pragma once
#include <temrixstd.h>
#include "XhciCommon.hpp"

static constexpr uint64_t XhciPortRegisterSetBaseOffset = 0x400;
static constexpr uint64_t XhciPortRegisterSetStride = 0x10;
static constexpr uint64_t XhciPortStatusAndControlRegisterOffset = 0x00;

static constexpr uint32_t PortStatusCurrentConnectStatusBit = 1u << 0;
static constexpr uint32_t PortStatusPortEnabledBit = 1u << 1;
static constexpr uint32_t PortStatusPortResetBit = 1u << 4;
static constexpr uint32_t PortStatusPortPowerBit = 1u << 9;
static constexpr uint32_t PortStatusConnectStatusChangeBit = 1u << 17;
static constexpr uint32_t PortStatusPortEnabledChangeBit = 1u << 18;
static constexpr uint32_t PortStatusPortResetChangeBit = 1u << 21;

static constexpr uint32_t PortStatusAllChangeBitsMask =
    PortStatusConnectStatusChangeBit | PortStatusPortEnabledChangeBit | PortStatusPortResetChangeBit |
    (1u << 19) /* warm reset change */ | (1u << 20) /* overcurrent change */ |
    (1u << 22) /* port link state change */ | (1u << 23) /* config error change */;

static inline uint64_t XhciPortRegisterSetBaseAddress(uint64_t operationalRegisterSetBaseAddress, uint32_t portNumberOneBased)
{
    return operationalRegisterSetBaseAddress + XhciPortRegisterSetBaseOffset + (portNumberOneBased - 1) * XhciPortRegisterSetStride;
}

static inline uint32_t XhciReadPortStatusAndControl(uint64_t operationalRegisterSetBaseAddress, uint32_t portNumberOneBased)
{
    uint64_t portRegisterSetBaseAddress = XhciPortRegisterSetBaseAddress(operationalRegisterSetBaseAddress, portNumberOneBased);
    return XhciReadRegister32(portRegisterSetBaseAddress, XhciPortStatusAndControlRegisterOffset);
}

static inline void XhciWritePortStatusAndControlPreservingReadWriteBits(uint64_t operationalRegisterSetBaseAddress, uint32_t portNumberOneBased, uint32_t bitsToSet)
{
    uint64_t portRegisterSetBaseAddress = XhciPortRegisterSetBaseAddress(operationalRegisterSetBaseAddress, portNumberOneBased);
    uint32_t currentValue = XhciReadRegister32(portRegisterSetBaseAddress, XhciPortStatusAndControlRegisterOffset);
    uint32_t writeValue = (currentValue & ~PortStatusAllChangeBitsMask & ~PortStatusPortEnabledBit) | bitsToSet;
    XhciWriteRegister32(portRegisterSetBaseAddress, XhciPortStatusAndControlRegisterOffset, writeValue);
}

static inline bool XhciPortIsConnected(uint32_t portStatusAndControlValue)
{
    return (portStatusAndControlValue & PortStatusCurrentConnectStatusBit) != 0;
}

static inline bool XhciPortIsEnabled(uint32_t portStatusAndControlValue)
{
    return (portStatusAndControlValue & PortStatusPortEnabledBit) != 0;
}

static inline uint32_t XhciPortGetSpeed(uint32_t portStatusAndControlValue)
{
    return (portStatusAndControlValue >> 10) & 0xF;
}

static inline void XhciPortClearConnectStatusChange(uint64_t operationalRegisterSetBaseAddress, uint32_t portNumberOneBased)
{
    XhciWritePortStatusAndControlPreservingReadWriteBits(operationalRegisterSetBaseAddress, portNumberOneBased, PortStatusConnectStatusChangeBit);
}

static inline void XhciPortClearResetChange(uint64_t operationalRegisterSetBaseAddress, uint32_t portNumberOneBased)
{
    XhciWritePortStatusAndControlPreservingReadWriteBits(operationalRegisterSetBaseAddress, portNumberOneBased, PortStatusPortResetChangeBit);
}

static inline bool XhciPortResetAndWaitForEnabled(uint64_t operationalRegisterSetBaseAddress, uint32_t portNumberOneBased)
{
    XhciWritePortStatusAndControlPreservingReadWriteBits(operationalRegisterSetBaseAddress, portNumberOneBased, PortStatusPortResetBit);

    for (uint32_t iteration = 0; iteration < 100000000; ++iteration)
    {
        uint32_t portStatusAndControlValue = XhciReadPortStatusAndControl(operationalRegisterSetBaseAddress, portNumberOneBased);
        if (portStatusAndControlValue & PortStatusPortResetChangeBit)
        {
            XhciPortClearResetChange(operationalRegisterSetBaseAddress, portNumberOneBased);
            return XhciPortIsEnabled(portStatusAndControlValue);
        }
    }

    return false;
}