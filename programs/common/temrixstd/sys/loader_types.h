#pragma once

#include <temrixstd/stdint.h>

struct Section {
    uint64_t virtualAddress;
    uint64_t fileOffset;
    uint64_t fileSize;
    uint64_t memorySize;
    uint32_t flags;
};

struct Relocation {
    uint64_t offset;
    int64_t  addend;
};

struct LoaderConfig {
    uint64_t baseMin;
    uint64_t baseMax;
    uint64_t alignment;
    uint64_t guardGap;
    uint64_t stackTop;
    uint64_t stackSize;
    bool debug;
};