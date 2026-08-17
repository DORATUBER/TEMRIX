bool initSuperblockAndGroups()
{
    superblockLba = partitionStart + TEMRIXFS_SUPERBLOCK_LBA_OFFSET;

    uint8_t sbBuf[SECTOR_SIZE];
    readSectors(superblockLba, 1, sbBuf);
    TemrixSuperblock *onDisk = reinterpret_cast<TemrixSuperblock *>(sbBuf);

    if (onDisk->magic == TEMRIXFS_SUPERBLOCK_MAGIC && onDisk->version == TEMRIXFS_SB_VERSION)
    {
        superblock = *onDisk;
        extentHeaderLba = superblock.extentHeaderLba;

        String::Printf("[temrixfs] superblock found: %u block group(s), root @ %llu\n",
                        superblock.blockGroupCount, superblock.rootDirHeaderLba);

        if (!loadBlockGroups())
            return false;

        if (!bindAllocatorFromDisk())
            return false;

        return true;
    }

    String::Printf("[temrixfs] no valid superblock, formatting fresh volume\n");
    return formatFreshVolume();
}

bool bindAllocatorFromDisk()
{
    blockIoAdapter.setOwner(this);
    allocator.init(SECTOR_SIZE, &blockIoAdapter);

    uint8_t extHdrBuf[SECTOR_SIZE];
    readSectors(extentHeaderLba, 1, extHdrBuf);
    ExtentTableHeader *extHdr = reinterpret_cast<ExtentTableHeader *>(extHdrBuf);

    if (!ExtentAllocator::isValidHeader(*extHdr))
    {
        String::Printf("[temrixfs] extent table header invalid despite valid superblock\n");
        return false;
    }

    allocator.loadHeader(*extHdr);

    uint64_t sectors = allocator.getMetadataSectorCount();
    uint32_t bytes   = (uint32_t)(sectors * SECTOR_SIZE);
    if (bytes > 0)
    {
        uint8_t *entryBuf = (uint8_t *)Syscall::Memory::Map(bytes);
        readMetadataBuffer(entryBuf, bytes);
        allocator.loadEntries(entryBuf, bytes);
    }

    String::Printf("[temrixfs] allocator bound, %u free extent(s)\n", allocator.freeExtentCount());
    return true;
}

bool formatFreshVolume()
{
    superblockLba = partitionStart + TEMRIXFS_SUPERBLOCK_LBA_OFFSET;
    uint64_t groupTableLba = superblockLba + 1;

    uint32_t provisionalGroups = estimateGroupCount(partitionEnd - groupTableLba + 1);
    uint32_t tableSectors      = groupTableSectors(provisionalGroups);

    extentHeaderLba   = groupTableLba + tableSectors;
    uint64_t firstFree = extentHeaderLba + 2; 

    if (firstFree > partitionEnd)
    {
        String::Printf("[temrixfs] partition too small for superblock + group table + allocator\n");
        return false;
    }

    uint32_t groupCount = estimateGroupCount(partitionEnd - firstFree + 1);
    blockGroupCount = groupCount;

    uint64_t cursor = firstFree;
    for (uint32_t g = 0; g < groupCount; g++)
    {
        uint64_t remaining = partitionEnd - cursor + 1;
        uint64_t size      = (remaining < TEMRIXFS_BLOCKS_PER_GROUP) ? remaining : TEMRIXFS_BLOCKS_PER_GROUP;

        blockGroups[g].startBlock    = cursor;
        blockGroups[g].blockCount    = size;
        blockGroups[g].freeBlockHint = size;
        blockGroups[g].flags         = 0;
        blockGroups[g].reserved      = 0;

        cursor += size;
    }
    blockGroups[0].flags |= TEMRIXFS_GROUP_FLAG_HAS_ROOT;

    blockIoAdapter.setOwner(this);
    allocator.init(SECTOR_SIZE, &blockIoAdapter);
    allocator.formatFresh(extentHeaderLba, firstFree, partitionEnd);

    if (!persistBlockGroups(groupTableLba))
        return false;

    FileNode root = allocateFileNode(0, /*isDirectory=*/true);
    if (root.selfLba == 0)
    {
        String::Printf("[temrixfs] formatFreshVolume: failed to allocate root directory\n");
        return false;
    }

    superblock.magic              = TEMRIXFS_SUPERBLOCK_MAGIC;
    superblock.version            = TEMRIXFS_SB_VERSION;
    superblock.sectorSize         = SECTOR_SIZE;
    superblock.partitionStart     = partitionStart;
    superblock.partitionEnd       = partitionEnd;
    superblock.totalBlocks        = partitionEnd - partitionStart + 1;
    superblock.extentHeaderLba    = extentHeaderLba;
    superblock.blockGroupTableLba = groupTableLba;
    superblock.blockGroupCount    = groupCount;
    superblock.blocksPerGroup     = TEMRIXFS_BLOCKS_PER_GROUP;
    superblock.rootDirHeaderLba   = root.selfLba;

    for (uint32_t i = 0; i < TEMRIXFS_VOLUME_LABEL_LEN; i++)
        superblock.volumeLabel[i] = 0;
    Memory::Copy((uint8_t *)superblock.volumeLabel, (const uint8_t *)"TEMRIXFS", 8);

    for (uint32_t i = 0; i < 8; i++)
        superblock.reserved[i] = 0;

    persistSuperblock();
    persistAllocatorState();

    String::Printf("[temrixfs] formatted fresh volume: %u block group(s), root @ %llu\n",
                    groupCount, root.selfLba);
    return true;
}

static uint32_t estimateGroupCount(uint64_t usableBlocks)
{
    uint32_t count = (uint32_t)((usableBlocks + TEMRIXFS_BLOCKS_PER_GROUP - 1) / TEMRIXFS_BLOCKS_PER_GROUP);
    if (count == 0)
        count = 1;
    if (count > TEMRIXFS_MAX_BLOCK_GROUPS)
        count = TEMRIXFS_MAX_BLOCK_GROUPS;
    return count;
}

static uint32_t groupTableSectors(uint32_t groupCount)
{
    uint32_t bytes = groupCount * (uint32_t)sizeof(BlockGroupDescriptor);
    return (bytes + SECTOR_SIZE - 1) / SECTOR_SIZE;
}

bool loadBlockGroups()
{
    blockGroupCount = superblock.blockGroupCount;
    if (blockGroupCount == 0 || blockGroupCount > TEMRIXFS_MAX_BLOCK_GROUPS)
    {
        String::Printf("[temrixfs] loadBlockGroups: bad group count %u\n", blockGroupCount);
        return false;
    }

    uint32_t bytes   = blockGroupCount * (uint32_t)sizeof(BlockGroupDescriptor);
    uint32_t sectors = (bytes + SECTOR_SIZE - 1) / SECTOR_SIZE;
    uint8_t *buf     = (uint8_t *)Syscall::Memory::Map(sectors * SECTOR_SIZE);

    readSectors(superblock.blockGroupTableLba, sectors, buf);
    Memory::Copy((uint8_t *)blockGroups, buf, bytes);

    return true;
}

bool persistBlockGroups(uint64_t lba)
{
    uint32_t bytes   = blockGroupCount * (uint32_t)sizeof(BlockGroupDescriptor);
    uint32_t sectors = (bytes + SECTOR_SIZE - 1) / SECTOR_SIZE;
    uint8_t *buf     = (uint8_t *)Syscall::Memory::Map(sectors * SECTOR_SIZE);

    for (uint32_t i = 0; i < sectors * SECTOR_SIZE; i++)
        buf[i] = 0;
    Memory::Copy(buf, (const uint8_t *)blockGroups, bytes);

    writeSectors(lba, sectors, buf);
    return true;
}

void persistSuperblock()
{
    uint8_t buf[SECTOR_SIZE];
    for (int i = 0; i < SECTOR_SIZE; i++)
        buf[i] = 0;
    Memory::Copy(buf, (const uint8_t *)&superblock, sizeof(TemrixSuperblock));
    writeSectors(superblockLba, 1, buf);
    nvme->flush();
}

uint32_t groupForBlock(uint64_t lba) const
{
    for (uint32_t g = 0; g < blockGroupCount; g++)
    {
        uint64_t end = blockGroups[g].startBlock + blockGroups[g].blockCount - 1;
        if (lba >= blockGroups[g].startBlock && lba <= end)
            return g;
    }
    return 0;
}