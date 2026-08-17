#pragma once

#include <temrixstd.h>
#include "nvme.hpp"
#include "FileSystem/gpt.hpp"
#include "TrxFsAllocator.hpp"
#include "BlockGroup.hpp"
#include "SuperBlock.hpp"
#include "DirectoryEntry.hpp"
#include "FileNode.hpp"

static constexpr Guid TEMRIX_TYPE_GUID = {
    0x812daf39, 0xbf7b, 0x4e6d, {0x9a, 0xcd, 0x88, 0x61, 0x83, 0x17, 0x4a, 0x9b}};

#define TEMRIXFS_MAX_ENTRY_SECTORS 128
#define TEMRIXFS_MAX_METADATA_EXTENTS 256

#define TEMRIXFS_SUPERBLOCK_LBA_OFFSET 0

#define TEMRIXFS_BLOCKS_PER_GROUP (32u * 1024u)

class TemrixFs
{
public:
#include "TreeApi.inc.hpp"
#include "FileApi.inc.hpp"

private:
    class BlockIoAdapter : public IMetadataBlockIo
    {
    public:
        void setOwner(TemrixFs *fs) { owner = fs; }

        bool readBlock(uint64_t lba, uint8_t *buf) override
        {
            owner->readSectors(lba, 1, buf);
            return true;
        }

        bool writeBlock(uint64_t lba, const uint8_t *buf) override
        {
            owner->writeSectors(lba, 1, (uint8_t *)buf);
            return true;
        }

    private:
        TemrixFs *owner = nullptr;
    };

    NvmeController *nvme;
    uint64_t partitionStart;
    uint64_t partitionEnd;
    uint64_t dmaBufVirt;
    uint64_t dmaBufPhys;
    uint32_t dmaBufSize;
    uint64_t prpListVirt;
    uint64_t prpListPhys;

    ExtentAllocator allocator;
    BlockIoAdapter blockIoAdapter;
    uint64_t extentHeaderLba;

    uint64_t superblockLba;
    TemrixSuperblock superblock;
    BlockGroupDescriptor blockGroups[TEMRIXFS_MAX_BLOCK_GROUPS];
    uint32_t blockGroupCount;

#include "SectorIo.inc.hpp"
#include "GptLocate.inc.hpp"
#include "SuperblockInit.inc.hpp"
#include "NodeAlloc.inc.hpp"
#include "DirectoryOps.inc.hpp"
#include "FileIoInternal.inc.hpp"
#include "DebugLog.inc.hpp"
};