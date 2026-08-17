#pragma once
#include <temrixstd.h>

static inline uint32_t XhciReadRegister32(uint64_t registerSetBaseAddress, uint64_t registerOffset)
{
    return *reinterpret_cast<volatile uint32_t *>(registerSetBaseAddress + registerOffset);
}

static inline void XhciWriteRegister32(uint64_t registerSetBaseAddress, uint64_t registerOffset, uint32_t value)
{
    *reinterpret_cast<volatile uint32_t *>(registerSetBaseAddress + registerOffset) = value;
}

static inline uint64_t XhciReadRegister64(uint64_t registerSetBaseAddress, uint64_t registerOffset)
{
    uint32_t lowDword = XhciReadRegister32(registerSetBaseAddress, registerOffset);
    uint32_t highDword = XhciReadRegister32(registerSetBaseAddress, registerOffset + 4);
    return ((uint64_t)highDword << 32) | lowDword;
}

static inline void XhciWriteRegister64(uint64_t registerSetBaseAddress, uint64_t registerOffset, uint64_t value)
{
    XhciWriteRegister32(registerSetBaseAddress, registerOffset, (uint32_t)(value & 0xFFFFFFFF));
    XhciWriteRegister32(registerSetBaseAddress, registerOffset + 4, (uint32_t)(value >> 32));
}

static constexpr uint64_t XhciDeviceMemoryAllocationFlags =
    ::Memory::Read | ::Memory::Write | ::Memory::User | ::Memory::NoCache;

static constexpr uint32_t CommandCompletionCodeSuccess = 1;
static constexpr uint32_t CommandCompletionCodeShortPacket = 13;