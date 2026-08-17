#pragma once
#include <temrixstd.h>

static constexpr uint32_t XhciMaxEndpointContextCount = 31;
static constexpr uint32_t XhciDeviceContextEntryCount = 1 + XhciMaxEndpointContextCount;
static constexpr uint32_t XhciInputContextEntryCount = 1 + XhciDeviceContextEntryCount;

struct SlotContext
{
    uint32_t dword0;
    uint32_t dword1;
    uint32_t dword2;
    uint32_t dword3;
    uint32_t dword4;
    uint32_t dword5;
    uint32_t dword6;
    uint32_t dword7;
};

struct EndpointContext
{
    uint32_t dword0;
    uint32_t dword1;
    uint32_t transferRingDequeuePointerLow;
    uint32_t transferRingDequeuePointerHigh;
    uint32_t dword4;
    uint32_t dword5;
    uint32_t dword6;
    uint32_t dword7;
};

struct InputControlContext
{
    uint32_t dropContextFlags;
    uint32_t addContextFlags;
    uint32_t reserved2;
    uint32_t reserved3;
    uint32_t reserved4;
    uint32_t reserved5;
    uint32_t reserved6;
    uint32_t configurationInterfaceAlternateSetting;
};

enum EndpointType : uint32_t
{
    EndpointTypeIsochronousOut = 1,
    EndpointTypeBulkOut        = 2,
    EndpointTypeInterruptOut   = 3,
    EndpointTypeControl        = 4,
    EndpointTypeIsochronousIn  = 5,
    EndpointTypeBulkIn         = 6,
    EndpointTypeInterruptIn    = 7,
};

static inline void SlotContextSetRouteString(SlotContext &slotContext, uint32_t routeString)
{
    slotContext.dword0 = (slotContext.dword0 & ~0xFFFFFu) | (routeString & 0xFFFFF);
}

static inline void SlotContextSetSpeed(SlotContext &slotContext, uint32_t portSpeed)
{
    slotContext.dword0 = (slotContext.dword0 & ~(0xFu << 20)) | ((portSpeed & 0xF) << 20);
}

static inline void SlotContextSetContextEntries(SlotContext &slotContext, uint32_t contextEntryCount)
{
    slotContext.dword0 = (slotContext.dword0 & ~(0x1Fu << 27)) | ((contextEntryCount & 0x1F) << 27);
}

static inline void SlotContextSetRootHubPortNumber(SlotContext &slotContext, uint32_t portNumberOneBased)
{
    slotContext.dword1 = (slotContext.dword1 & ~(0xFFu << 16)) | ((portNumberOneBased & 0xFF) << 16);
}

static inline void EndpointContextSetInterval(EndpointContext &endpointContext, uint32_t interval)
{
    endpointContext.dword0 = (endpointContext.dword0 & ~0xFFu) | (interval & 0xFF);
}

static inline void EndpointContextSetErrorCount(EndpointContext &endpointContext, uint32_t errorCount)
{
    endpointContext.dword1 = (endpointContext.dword1 & ~(0x3u << 1)) | ((errorCount & 0x3) << 1);
}

static inline void EndpointContextSetEndpointType(EndpointContext &endpointContext, uint32_t endpointType)
{
    endpointContext.dword1 = (endpointContext.dword1 & ~(0x7u << 3)) | ((endpointType & 0x7) << 3);
}

static inline void EndpointContextSetMaxPacketSize(EndpointContext &endpointContext, uint32_t maxPacketSize)
{
    endpointContext.dword1 = (endpointContext.dword1 & ~(0xFFFFu << 16)) | ((maxPacketSize & 0xFFFF) << 16);
}

static inline void EndpointContextSetTransferRingDequeuePointer(EndpointContext &endpointContext, uint64_t transferRingPhysicalAddress, uint32_t initialCycleState)
{
    endpointContext.transferRingDequeuePointerLow = (uint32_t)(transferRingPhysicalAddress & 0xFFFFFFFF) | (initialCycleState & 0x1);
    endpointContext.transferRingDequeuePointerHigh = (uint32_t)(transferRingPhysicalAddress >> 32);
}

static inline void EndpointContextSetAverageTransferRequestBlockLength(EndpointContext &endpointContext, uint32_t averageTransferRequestBlockLength)
{
    endpointContext.dword4 = (endpointContext.dword4 & ~0xFFFFu) | (averageTransferRequestBlockLength & 0xFFFF);
}

static inline uint32_t XhciMaxPacketSizeForPortSpeed(uint32_t portSpeed)
{
    switch (portSpeed)
    {
    case 1: return 64;  
    case 2: return 8;   
    case 3: return 64;  
    case 4: return 512; 
    case 5: return 512; 
    default: return 8;
    }
}