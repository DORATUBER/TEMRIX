FileExtentHeader createFile()
{
    FileExtentHeader file;
    file.magic     = TEMRIXFS_FILE_MAGIC;
    file.version    = 1;
    file.reserved   = 0;
    file.sizeBytes  = 0;

    file.data.directCount = 0;
    file.data.reserved0   = 0;
    file.data.l1Lba       = 0;
    file.data.l2Lba       = 0;
    file.data.l3Lba       = 0;

    return file;
}

void deleteFile(FileExtentHeader &file)
{
    allocator.freeDescriptor(file.data);
    file.sizeBytes = 0;
    persistAllocatorState();

    String::Printf("[temrixfs] deleteFile: freed all blocks\n");
}

bool writeFile(FileExtentHeader &file, uint64_t offset, const uint8_t *buf, uint32_t len)
{
    if (len == 0)
        return true;

    uint64_t requiredBytes  = offset + len;
    uint64_t currentBlocks  = descriptorBlockCount(file.data);
    uint64_t requiredBlocks = (requiredBytes + SECTOR_SIZE - 1) / SECTOR_SIZE;

    if (requiredBlocks > currentBlocks)
    {
        if (!allocator.growDescriptor(file.data, requiredBlocks - currentBlocks))
        {
            String::Printf("[temrixfs] writeFile: failed to grow file to %llu blocks\n", requiredBlocks);
            return false;
        }

        persistAllocatorState();
    }

    bool ok = ioFile(file, offset, (uint8_t *)buf, len, /*write=*/true);

    if (ok && requiredBytes > file.sizeBytes)
        file.sizeBytes = requiredBytes;

    return ok;
}

bool writeFileInGroup(FileExtentHeader &file, uint64_t offset, const uint8_t *buf, uint32_t len,
                       uint32_t preferredGroup)
{
    if (len == 0)
        return true;

    uint64_t requiredBytes  = offset + len;
    uint64_t currentBlocks  = descriptorBlockCount(file.data);
    uint64_t requiredBlocks = (requiredBytes + SECTOR_SIZE - 1) / SECTOR_SIZE;

    if (requiredBlocks > currentBlocks)
    {
        if (!allocator.growDescriptorInGroups(file.data, requiredBlocks - currentBlocks,
                                               blockGroups, blockGroupCount, preferredGroup))
        {
            String::Printf("[temrixfs] writeFileInGroup: failed to grow to %llu blocks (prefer group %u)\n",
                            requiredBlocks, preferredGroup);
            return false;
        }

        persistAllocatorState();
    }

    bool ok = ioFile(file, offset, (uint8_t *)buf, len, /*write=*/true);

    if (ok && requiredBytes > file.sizeBytes)
        file.sizeBytes = requiredBytes;

    return ok;
}

bool readFile(FileExtentHeader &file, uint64_t offset, uint8_t *buf, uint32_t len)
{
    if (len == 0)
        return true;

    if (offset >= file.sizeBytes)
    {
        for (uint32_t i = 0; i < len; i++)
            buf[i] = 0;
        return true;
    }

    uint64_t avail  = file.sizeBytes - offset;
    uint32_t toRead = (avail < (uint64_t)len) ? (uint32_t)avail : len;

    bool ok = ioFile(file, offset, buf, toRead, /*write=*/false);

    for (uint32_t i = toRead; i < len; i++)
        buf[i] = 0;

    return ok;
}

bool truncateFile(FileExtentHeader &file, uint64_t newSize)
{
    if (newSize >= file.sizeBytes)
    {
        file.sizeBytes = newSize;
        return true;
    }

    uint64_t newBlocks = (newSize + SECTOR_SIZE - 1) / SECTOR_SIZE;

    allocator.shrinkDescriptor(file.data, newBlocks);
    file.sizeBytes = newSize;
    persistAllocatorState();

    return true;
}

void writeFileHeader(const FileExtentHeader &file, uint64_t lba)
{
    uint8_t buf[SECTOR_SIZE];
    for (int b = 0; b < SECTOR_SIZE; b++)
        buf[b] = 0;
    Memory::Copy(buf, (const uint8_t *)&file, sizeof(FileExtentHeader));
    writeSectors(lba, 1, buf);
}

bool readFileHeader(FileExtentHeader &file, uint64_t lba)
{
    uint8_t buf[SECTOR_SIZE];
    readSectors(lba, 1, buf);
    Memory::Copy((uint8_t *)&file, buf, sizeof(FileExtentHeader));
    return file.magic == TEMRIXFS_FILE_MAGIC;
}