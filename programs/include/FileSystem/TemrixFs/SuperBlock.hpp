#pragma once

#include <temrixstd.h>
#include "BlockGroup.hpp"

#define TEMRIXFS_SUPERBLOCK_MAGIC 0x585346585254544Eull 
#define TEMRIXFS_SB_VERSION       1

#define TEMRIXFS_MAX_BLOCK_GROUPS 512

#define TEMRIXFS_VOLUME_LABEL_LEN 32

struct TemrixSuperblock
{
    uint64_t magic;
    uint32_t version;
    uint32_t sectorSize;

    uint64_t partitionStart;
    uint64_t partitionEnd;
    uint64_t totalBlocks;

    uint64_t extentHeaderLba;  
    uint64_t blockGroupTableLba; 
    uint32_t blockGroupCount;
    uint32_t blocksPerGroup;    

    uint64_t rootDirHeaderLba; 

    char     volumeLabel[TEMRIXFS_VOLUME_LABEL_LEN];

    uint32_t reserved[8];
};