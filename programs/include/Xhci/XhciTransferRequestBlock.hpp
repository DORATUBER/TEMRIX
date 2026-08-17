#pragma once
#include <temrixstd.h>

struct TransferRequestBlock
{
    uint32_t parameterLow;
    uint32_t parameterHigh;
    uint32_t status;
    uint32_t control;
};
static_assert(sizeof(TransferRequestBlock) == 16, "TransferRequestBlock must be 16 bytes");

enum TransferRequestBlockType : uint32_t
{
    TransferRequestBlockTypeNormal = 1,
    TransferRequestBlockTypeSetupStage = 2,
    TransferRequestBlockTypeDataStage = 3,
    TransferRequestBlockTypeStatusStage = 4,
    TransferRequestBlockTypeLink = 6,
    TransferRequestBlockTypeEnableSlotCommand = 9,
    TransferRequestBlockTypeAddressDeviceCommand = 11,
    TransferRequestBlockTypeConfigureEndpointCommand = 12,
    TransferRequestBlockTypeNoOpCommand = 23,
    TransferRequestBlockTypeTransferEvent = 32,
    TransferRequestBlockTypeCommandCompletionEvent = 33,
    TransferRequestBlockTypePortStatusChangeEvent = 34,
};

static inline uint32_t TransferRequestBlockControlWithType(uint32_t transferRequestBlockType)
{
    return (transferRequestBlockType & 0x3F) << 10;
}

static inline uint32_t TransferRequestBlockGetType(const TransferRequestBlock &transferRequestBlock)
{
    return (transferRequestBlock.control >> 10) & 0x3F;
}

static inline bool TransferRequestBlockGetCycleBit(const TransferRequestBlock &transferRequestBlock)
{
    return (transferRequestBlock.control & 0x1) != 0;
}