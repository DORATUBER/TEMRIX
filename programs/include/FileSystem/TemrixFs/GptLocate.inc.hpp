bool locateTemrixPartition()
{
    uint8_t headerBuf[SECTOR_SIZE];
    readSectors(GPT_HEADER_LBA, 1, headerBuf);
    GptHeader *hdr = reinterpret_cast<GptHeader *>(headerBuf);

    logGptHeader(hdr);

    if (Memory::Compare(hdr->signature, "EFI PART", 8) != 0)
    {
        String::Printf("[temrixfs] no GPT\n");
        return false;
    }

    uint32_t entrySize    = hdr->partitionEntrySize;
    uint32_t totalEntries = hdr->numPartitionEntries;
    uint64_t entryLba     = hdr->partitionEntryLba;

    uint64_t totalBytes    = (uint64_t)entrySize * totalEntries;
    uint32_t sectorsToRead = (uint32_t)((totalBytes + SECTOR_SIZE - 1) / SECTOR_SIZE);

    if (sectorsToRead == 0 || sectorsToRead > TEMRIXFS_MAX_ENTRY_SECTORS)
    {
        String::Printf("[temrixfs] bad partition entry count (sectorsToRead=%u)\n", sectorsToRead);
        return false;
    }

    uint8_t *entryBuf = (uint8_t *)Syscall::Memory::Map(sectorsToRead * SECTOR_SIZE);
    readSectors(entryLba, sectorsToRead, entryBuf);

    String::Printf("[temrixfs] --- partition entries ---\n");
    for (uint32_t i = 0; i < totalEntries; i++)
    {
        GptPartitionEntry *entry =
            reinterpret_cast<GptPartitionEntry *>(entryBuf + i * entrySize);

        if (isEmptyEntry(entry))
            continue;

        bool isTemrix = entry->partitionTypeGuid == TEMRIX_TYPE_GUID;
        logPartitionEntry(i, entry, isTemrix);

        if (isTemrix && !partitionStart)
        {
            partitionStart = entry->startingLba;
            partitionEnd   = entry->endingLba;
        }
    }

    if (!partitionStart)
    {
        String::Printf("[temrixfs] partition not found\n");
        return false;
    }

    String::Printf("[temrixfs] init OK, partitionStart=%llu, partitionEnd=%llu\n",
                    partitionStart, partitionEnd);
    return true;
}

static bool isEmptyEntry(const GptPartitionEntry *entry)
{
    if (entry->partitionTypeGuid.data1 || entry->partitionTypeGuid.data2 || entry->partitionTypeGuid.data3)
        return false;

    for (int j = 0; j < 8; j++)
        if (entry->partitionTypeGuid.data4[j])
            return false;

    return true;
}