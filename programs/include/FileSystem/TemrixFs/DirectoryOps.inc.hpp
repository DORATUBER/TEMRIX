static void fillDirEntry(DirEntry &rec, const char *name, uint32_t nameLen, uint8_t type, uint64_t headerLba)
{
    rec.type      = type;
    rec.nameLen   = (uint8_t)nameLen;
    rec.reserved  = 0;
    rec.reserved1 = 0;
    rec.headerLba = headerLba;

    uint32_t i = 0;
    for (; i < nameLen; i++)
        rec.name[i] = name[i];
    for (; i < TEMRIXFS_DIRENT_NAME_LEN; i++)
        rec.name[i] = '\0';
}

static bool direntNameEquals(const DirEntry &rec, const char *name, uint32_t nameLen)
{
    if (rec.nameLen != nameLen)
        return false;

    for (uint32_t i = 0; i < nameLen; i++)
        if (rec.name[i] != name[i])
            return false;

    return true;
}

static uint32_t strLenBounded(const char *s, uint32_t maxLen)
{
    uint32_t n = 0;
    while (s[n] != '\0' && n < maxLen)
        n++;
    return n;
}

bool dirFind(FileNode &dir, const char *name, DirEntry &out)
{
    uint32_t nameLen   = strLenBounded(name, TEMRIXFS_DIRENT_NAME_LEN - 1);
    uint64_t entryCount = dir.header.sizeBytes / sizeof(DirEntry);

    for (uint64_t i = 0; i < entryCount; i++)
    {
        if (!readFile(dir.header, i * sizeof(DirEntry), (uint8_t *)&out, sizeof(DirEntry)))
            return false;

        if (out.type != TEMRIXFS_DIRENT_TYPE_FREE && direntNameEquals(out, name, nameLen))
            return true;
    }

    return false;
}

uint32_t dirList(FileNode &dir, DirEntry *out, uint32_t maxOut)
{
    uint64_t entryCount = dir.header.sizeBytes / sizeof(DirEntry);
    uint32_t written    = 0;

    for (uint64_t i = 0; i < entryCount; i++)
    {
        DirEntry rec;
        if (!readFile(dir.header, i * sizeof(DirEntry), (uint8_t *)&rec, sizeof(DirEntry)))
            break;

        if (rec.type == TEMRIXFS_DIRENT_TYPE_FREE)
            continue;

        if (written < maxOut)
            out[written] = rec;
        written++;
    }

    return written;
}

bool dirAddEntry(FileNode &dir, const char *name, uint8_t type, uint64_t childLba)
{
    uint32_t nameLen = strLenBounded(name, TEMRIXFS_DIRENT_NAME_LEN - 1);
    uint64_t entryCount = dir.header.sizeBytes / sizeof(DirEntry);

    DirEntry rec;

    
    
    for (uint64_t i = 0; i < entryCount; i++)
    {
        if (!readFile(dir.header, i * sizeof(DirEntry), (uint8_t *)&rec, sizeof(DirEntry)))
            return false;

        if (rec.type == TEMRIXFS_DIRENT_TYPE_FREE)
        {
            fillDirEntry(rec, name, nameLen, type, childLba);
            return writeDirEntry(dir, i, rec);
        }

        if (direntNameEquals(rec, name, nameLen))
        {
            String::Printf("[temrixfs] dirAddEntry: '%s' already exists\n", name);
            return false;
        }
    }

    fillDirEntry(rec, name, nameLen, type, childLba);
    return writeDirEntry(dir, entryCount, rec);
}

bool dirRemoveEntry(FileNode &dir, const char *name)
{
    uint32_t nameLen   = strLenBounded(name, TEMRIXFS_DIRENT_NAME_LEN - 1);
    uint64_t entryCount = dir.header.sizeBytes / sizeof(DirEntry);

    for (uint64_t i = 0; i < entryCount; i++)
    {
        DirEntry rec;
        if (!readFile(dir.header, i * sizeof(DirEntry), (uint8_t *)&rec, sizeof(DirEntry)))
            return false;

        if (rec.type != TEMRIXFS_DIRENT_TYPE_FREE && direntNameEquals(rec, name, nameLen))
        {
            DirEntry tombstone;
            fillDirEntry(tombstone, "", 0, TEMRIXFS_DIRENT_TYPE_FREE, 0);
            if (!writeDirEntry(dir, i, tombstone))
                return false;

            shrinkTrailingFreeEntries(dir);
            return true;
        }
    }

    String::Printf("[temrixfs] dirRemoveEntry: '%s' not found\n", name);
    return false;
}

void shrinkTrailingFreeEntries(FileNode &dir)
{
    uint64_t entryCount = dir.header.sizeBytes / sizeof(DirEntry);
    uint64_t newCount    = entryCount;

    while (newCount > 0)
    {
        DirEntry rec;
        if (!readFile(dir.header, (newCount - 1) * sizeof(DirEntry), (uint8_t *)&rec, sizeof(DirEntry)))
            break;

        if (rec.type != TEMRIXFS_DIRENT_TYPE_FREE)
            break;

        newCount--;
    }

    if (newCount == entryCount)
        return; 

    truncateFileNode(dir, newCount * sizeof(DirEntry));
}

bool writeDirEntry(FileNode &dir, uint64_t index, const DirEntry &rec)
{
    uint32_t preferredGroup = groupForBlock(dir.selfLba);
    if (!writeFileInGroup(dir.header, index * sizeof(DirEntry), (const uint8_t *)&rec, sizeof(DirEntry), preferredGroup))
        return false;

    writeFileHeader(dir.header, dir.selfLba);
    return true;
}