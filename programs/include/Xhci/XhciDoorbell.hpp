#pragma once
#include <temrixstd.h>
#include "XhciCommon.hpp"

static inline void XhciRingDoorbell(uint64_t doorbellRegisterSetBaseAddress, uint32_t doorbellIndex, uint32_t targetEndpointValue = 0, uint32_t streamId = 0)
{
    uint32_t doorbellValue = targetEndpointValue | (streamId << 16);
    XhciWriteRegister32(doorbellRegisterSetBaseAddress, doorbellIndex * 4, doorbellValue);
}