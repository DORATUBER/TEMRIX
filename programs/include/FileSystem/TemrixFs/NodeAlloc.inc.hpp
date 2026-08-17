FileNode allocateFileNode(uint32_t preferredGroup, bool isDirectory)
{
    FileNode node;
    node.header      = createFile();
    node.isDirectory = isDirectory;
    node.selfLba     = 0;

    Extent hdrExt = allocator.allocateInGroup(1, blockGroups, blockGroupCount, preferredGroup);
    if (!hdrExt.valid())
    {
        String::Printf("[temrixfs] allocateFileNode: no space for header sector (prefer group %u)\n",
                        preferredGroup);
        return node;
    }

    node.selfLba = hdrExt.startBlock;
    writeFileHeader(node.header, node.selfLba);
    persistAllocatorState();

    return node;
}

void freeFileNode(FileNode &node)
{
    allocator.freeDescriptor(node.header.data);
    allocator.free(Extent{node.selfLba, 1});
    persistAllocatorState();
    node.selfLba = 0;
}