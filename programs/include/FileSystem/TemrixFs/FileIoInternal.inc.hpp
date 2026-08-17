void persistAllocatorState()
{
    uint8_t hdrBuf[SECTOR_SIZE];
    for (int b = 0; b < SECTOR_SIZE; b++)
        hdrBuf[b] = 0;
    Memory::Copy(hdrBuf, (const uint8_t *)&allocator.getHeader(), sizeof(ExtentTableHeader));
    writeSectors(extentHeaderLba, 1, hdrBuf);

    uint64_t sectors = allocator.getMetadataSectorCount();
    uint32_t bytes   = (uint32_t)(sectors * SECTOR_SIZE);
    if (bytes > 0)
    {
        uint8_t *buf = (uint8_t *)Syscall::Memory::Map(bytes);
        allocator.serializeEntries(buf, bytes);
        writeMetadataBuffer(buf, bytes);
    }

    nvme->flush();
}

void readMetadataBuffer(uint8_t *buf, uint32_t bytes)
{
    Extent layout[TEMRIXFS_MAX_METADATA_EXTENTS];
    uint32_t n = allocator.enumerateMetadataExtents(layout, TEMRIXFS_MAX_METADATA_EXTENTS);

    uint32_t offset = 0;
    for (uint32_t i = 0; i < n && offset < bytes; i++)
    {
        uint32_t chunkBytes = (uint32_t)layout[i].blockCount * SECTOR_SIZE;
        readSectors(layout[i].startBlock, (uint32_t)layout[i].blockCount, buf + offset);
        offset += chunkBytes;
    }

    if (offset < bytes)
        String::Printf("[temrixfs] WARNING: metadata layout only covered %u/%u bytes\n", offset, bytes);
}

void writeMetadataBuffer(uint8_t *buf, uint32_t bytes)
{
    Extent layout[TEMRIXFS_MAX_METADATA_EXTENTS];
    uint32_t n = allocator.enumerateMetadataExtents(layout, TEMRIXFS_MAX_METADATA_EXTENTS);

    uint32_t offset = 0;
    for (uint32_t i = 0; i < n && offset < bytes; i++)
    {
        uint32_t chunkBytes = (uint32_t)layout[i].blockCount * SECTOR_SIZE;
        writeSectors(layout[i].startBlock, (uint32_t)layout[i].blockCount, buf + offset);
        offset += chunkBytes;
    }

    if (offset < bytes)
        String::Printf("[temrixfs] WARNING: metadata layout only covered %u/%u bytes\n", offset, bytes);
}

uint64_t descriptorBlockCount(const ExtentDescriptor &desc) const
{
    Extent layout[TEMRIXFS_MAX_METADATA_EXTENTS];
    uint32_t n = allocator.enumerateDescriptor(desc, layout, TEMRIXFS_MAX_METADATA_EXTENTS);

    uint64_t total = 0;
    for (uint32_t i = 0; i < n; i++)
        total += layout[i].blockCount;

    return total;
}

bool ioFile(FileExtentHeader &file, uint64_t offset, uint8_t *buf, uint32_t len, bool write)
{
    Extent layout[TEMRIXFS_MAX_METADATA_EXTENTS];
    uint32_t n = allocator.enumerateDescriptor(file.data, layout, TEMRIXFS_MAX_METADATA_EXTENTS);

    uint64_t startBlock  = offset / SECTOR_SIZE;
    uint64_t endBlock    = (offset + len - 1) / SECTOR_SIZE; 
    uint64_t blockCursor = 0;
    uint32_t bufCursor   = 0;
    uint64_t byteCursor  = offset;
    uint32_t remaining   = len;

    uint8_t sectorBuf[SECTOR_SIZE];

    for (uint32_t i = 0; i < n && remaining > 0; i++)
    {
        uint64_t extBlockStart = blockCursor;
        uint64_t extBlockEnd   = blockCursor + layout[i].blockCount - 1;

        if (endBlock < extBlockStart || startBlock > extBlockEnd)
        {
            blockCursor += layout[i].blockCount;
            continue;
        }

        uint64_t blk = (startBlock > extBlockStart) ? startBlock : extBlockStart;

        while (blk <= extBlockEnd && remaining > 0)
        {
            uint64_t sectorByteStart = blk * SECTOR_SIZE;
            uint32_t inSectorOffset  = (uint32_t)(byteCursor - sectorByteStart);
            uint32_t inSectorLen     = SECTOR_SIZE - inSectorOffset;
            if (inSectorLen > remaining)
                inSectorLen = remaining;

            uint64_t lba = layout[i].startBlock + (blk - extBlockStart);

            if (write)
            {
                if (inSectorOffset != 0 || inSectorLen != SECTOR_SIZE)
                {
                    readSectors(lba, 1, sectorBuf);
                    Memory::Copy(sectorBuf + inSectorOffset, buf + bufCursor, inSectorLen);
                    writeSectors(lba, 1, sectorBuf);
                }
                else
                {
                    writeSectors(lba, 1, buf + bufCursor);
                }
            }
            else
            {
                readSectors(lba, 1, sectorBuf);
                Memory::Copy(buf + bufCursor, sectorBuf + inSectorOffset, inSectorLen);
            }

            bufCursor  += inSectorLen;
            byteCursor += inSectorLen;
            remaining  -= inSectorLen;
            blk++;
        }

        blockCursor += layout[i].blockCount;
    }

    if (remaining != 0)
    {
        String::Printf("[temrixfs] ioFile: incomplete %s, %u bytes unresolved (file too short?)\n",
                        write ? "write" : "read", remaining);
        return false;
    }

    return true;
}