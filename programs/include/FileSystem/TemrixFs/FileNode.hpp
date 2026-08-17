#pragma once

#include <temrixstd.h>
#include "TrxFsAllocator.hpp"

struct FileNode
{
    FileExtentHeader header;
    uint64_t         selfLba;
    bool             isDirectory;
};