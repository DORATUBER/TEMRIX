bool init(NvmeController *nvme)
{
    this->nvme            = nvme;
    this->dmaBufVirt      = 0;
    this->dmaBufPhys      = 0;
    this->dmaBufSize      = 0;
    this->partitionStart  = 0;
    this->partitionEnd    = 0;
    this->extentHeaderLba = 0;
    this->blockGroupCount = 0;

    if (!setupPrpList())
        return false;

    if (!locateTemrixPartition())
        return false;

    if (!initSuperblockAndGroups())
    {
        String::Printf("[temrixfs] superblock/group init FAILED\n");
        return false;
    }

    return true;
}

uint64_t getPartitionStart() const { return partitionStart; }
uint64_t getPartitionEnd() const { return partitionEnd; }
uint32_t getBlockGroupCount() const { return blockGroupCount; }

Extent allocateBlocks(uint64_t count)
{
    Extent result = allocator.allocate(count);
    if (result.valid())
        persistAllocatorState();
    return result;
}

void freeBlocks(Extent ext)
{
    if (!ext.valid())
        return;

    allocator.free(ext);
    persistAllocatorState();
}

uint32_t freeExtentCount() const { return allocator.freeExtentCount(); }
uint64_t freeBlockTotal() const { return allocator.freeBlockTotal(); }

bool getRootDirectory(FileNode &out)
{
    out.selfLba     = superblock.rootDirHeaderLba;
    out.isDirectory = true;
    return readFileHeader(out.header, out.selfLba);
}

bool createDirectory(FileNode &parent, const char *name, FileNode &outChild)
{
    if (!parent.isDirectory)
    {
        String::Printf("[temrixfs] createDirectory: parent is not a directory\n");
        return false;
    }

    DirEntry existing;
    if (dirFind(parent, name, existing))
    {
        String::Printf("[temrixfs] createDirectory: '%s' already exists\n", name);
        return false;
    }

    uint32_t preferredGroup = groupForBlock(parent.selfLba);
    outChild = allocateFileNode(preferredGroup, /*isDirectory=*/true);
    if (outChild.selfLba == 0)
        return false;

    if (!dirAddEntry(parent, name, TEMRIXFS_DIRENT_TYPE_DIR, outChild.selfLba))
    {
        freeFileNode(outChild);
        return false;
    }

    return true;
}

bool createRegularFile(FileNode &parent, const char *name, FileNode &outChild)
{
    if (!parent.isDirectory)
    {
        String::Printf("[temrixfs] createRegularFile: parent is not a directory\n");
        return false;
    }

    DirEntry existing;
    if (dirFind(parent, name, existing))
    {
        String::Printf("[temrixfs] createRegularFile: '%s' already exists\n", name);
        return false;
    }

    uint32_t preferredGroup = groupForBlock(parent.selfLba);
    outChild = allocateFileNode(preferredGroup, /*isDirectory=*/false);
    if (outChild.selfLba == 0)
        return false;

    if (!dirAddEntry(parent, name, TEMRIXFS_DIRENT_TYPE_FILE, outChild.selfLba))
    {
        freeFileNode(outChild);
        return false;
    }

    return true;
}

bool lookup(FileNode &dir, const char *name, FileNode &out)
{
    DirEntry entry;
    if (!dirFind(dir, name, entry))
        return false;

    out.selfLba     = entry.headerLba;
    out.isDirectory = (entry.type == TEMRIXFS_DIRENT_TYPE_DIR);
    return readFileHeader(out.header, out.selfLba);
}

bool resolvePath(const char *path, FileNode &out)
{
    if (!getRootDirectory(out))
        return false;

    if (path == nullptr || path[0] == '\0' || (path[0] == '/' && path[1] == '\0'))
        return true;

    uint32_t i = (path[0] == '/') ? 1 : 0;
    char component[TEMRIXFS_DIRENT_NAME_LEN];

    while (path[i] != '\0')
    {
        uint32_t c = 0;
        while (path[i] != '\0' && path[i] != '/' && c < TEMRIXFS_DIRENT_NAME_LEN - 1)
            component[c++] = path[i++];
        component[c] = '\0';

        if (path[i] == '/')
            i++;

        if (c == 0)
            continue;

        FileNode next;
        if (!lookup(out, component, next))
            return false;

        out = next;
    }

    return true;
}

bool remove(FileNode &parent, const char *name)
{
    DirEntry entry;
    if (!dirFind(parent, name, entry))
    {
        String::Printf("[temrixfs] remove: '%s' not found\n", name);
        return false;
    }

    FileNode child;
    child.selfLba     = entry.headerLba;
    child.isDirectory = (entry.type == TEMRIXFS_DIRENT_TYPE_DIR);
    if (!readFileHeader(child.header, child.selfLba))
        return false;

    if (child.isDirectory)
    {
        DirEntry scratch[1];
        if (dirList(child, scratch, 1) != 0)
        {
            String::Printf("[temrixfs] remove: '%s' is a non-empty directory\n", name);
            return false;
        }
    }

    freeFileNode(child);
    return dirRemoveEntry(parent, name);
}

uint32_t listDirectory(FileNode &dir, DirEntry *out, uint32_t maxOut)
{
    return dirList(dir, out, maxOut);
}

bool writeFileNode(FileNode &node, uint64_t offset, const uint8_t *buf, uint32_t len)
{
    uint32_t preferredGroup = groupForBlock(node.selfLba);
    if (!writeFileInGroup(node.header, offset, buf, len, preferredGroup))
        return false;

    writeFileHeader(node.header, node.selfLba);
    return true;
}

bool readFileNode(FileNode &node, uint64_t offset, uint8_t *buf, uint32_t len)
{
    return readFile(node.header, offset, buf, len);
}

bool truncateFileNode(FileNode &node, uint64_t newSize)
{
    if (!truncateFile(node.header, newSize))
        return false;

    writeFileHeader(node.header, node.selfLba);
    return true;
}

void deleteFileNode(FileNode &node)
{
    deleteFile(node.header);
}