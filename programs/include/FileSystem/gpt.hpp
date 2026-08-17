#pragma once

#include <temrixstd.h>

#define GPT_HEADER_LBA              1
#define GPT_SIG_LEN                 8
#define GPT_OFF_ENTRY_LBA           72
#define GPT_OFF_NUM_PARTS           80
#define GPT_OFF_ENTRY_SIZE          84
#define GPT_PART_OFF_TYPE_GUID      0
#define GPT_PART_OFF_START_LBA      32
#define GPT_GUID_LEN                16

struct Guid
{
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t  data4[8];

    bool operator==(const Guid &other) const
    {
        return data1 == other.data1 &&
               data2 == other.data2 &&
               data3 == other.data3 &&
               Memory::Compare(data4, other.data4, 8) == 0;
    }
};

#pragma pack(push, 1)
struct GptHeader
{
    char     signature[8]; 
    uint32_t revision;
    uint32_t headerSize;
    uint32_t headerCrc32;
    uint32_t reserved;
    uint64_t myLba;
    uint64_t alternateLba;
    uint64_t firstUsableLba;
    uint64_t lastUsableLba;
    uint8_t  diskGuid[16];
    uint64_t partitionEntryLba;
    uint32_t numPartitionEntries;
    uint32_t partitionEntrySize;
    uint32_t partitionArrayCrc32;
};

struct GptPartitionEntry
{
    Guid     partitionTypeGuid;
    Guid     uniquePartitionGuid;
    uint64_t startingLba;
    uint64_t endingLba;
    uint64_t attributes;
    uint16_t name[36]; 
};
#pragma pack(pop)