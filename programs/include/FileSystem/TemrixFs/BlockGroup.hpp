#pragma once

#include <temrixstd.h>

struct BlockGroupDescriptor
{
    uint64_t startBlock;
    uint64_t blockCount;
    uint64_t freeBlockHint;
    uint32_t flags;
    uint32_t reserved;
};

#define TEMRIXFS_GROUP_FLAG_HAS_ROOT 0x1u