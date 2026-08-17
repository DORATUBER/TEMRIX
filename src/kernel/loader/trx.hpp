#pragma once
#include "common.hpp"
#include "task.hpp"
#include "loader.hpp"

#pragma pack(push, 1)

constexpr uint32_t TRX_SEC_READ  = 0x1;
constexpr uint32_t TRX_SEC_WRITE = 0x2;
constexpr uint32_t TRX_SEC_EXEC  = 0x4;

struct TrxHeader {
    char     magic[4];
    uint32_t version;
    uint64_t entry;
    uint32_t numSections;
    uint32_t sectionTableOffset;
    uint32_t relocTableOffset;
    uint32_t numRelocs;
};

struct TrxSection {
    uint64_t vaddr;
    uint64_t fileOffset;
    uint64_t fileSize;
    uint64_t memSize;
    uint32_t flags;
    uint32_t reserved;
};

#pragma pack(pop)

using TrxReloc = KernelReloc;

namespace TRX {
    bool parse(const uint8_t* binary, uint64_t size, KernelSpawnInfo* out);
    bool load(const uint8_t* binary, uint64_t size, Task** outTask);
}