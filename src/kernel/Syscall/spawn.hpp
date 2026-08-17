#pragma once
#include "common.hpp"

static constexpr uint32_t MAX_SPAWN_MAPS = 64;
static constexpr uint32_t MAX_SPAWN_COPIES = 64;
static constexpr uint32_t MAX_MMAP_SIZE = 256 * 1024 * 1024;
static constexpr uint32_t MAX_WRITE_LEN = 4096;
static constexpr uint32_t RESERVED_MEMORY = 4 * 1024 * 1024;

struct MapDescriptor
{
    uint64_t dst;
    uint64_t pageCount;
    uint64_t flags;
    bool zero;
};

struct CopyDescriptor
{
    uint64_t src;
    uint64_t dst;
    uint64_t size;
};

enum class SpawnReg : uint8_t
{
    RDI,
    RSI,
    RDX,
    RCX,
    R8,
    R9,
    RBX,
    RBP,
    R12,
    R13,
    R14,
    R15,
    Count
};

struct RegisterValue
{
    uint8_t regIndex;
    uint64_t value;
};

struct SpawnInfo
{
    MapDescriptor *maps;
    uint32_t numMaps;
    CopyDescriptor *copies;
    uint32_t numCopies;
    RegisterValue *registers;
    uint32_t numRegisters;
    uint64_t entry;
    uint64_t stackPointer;
    uint64_t requestedCapabilities = 0;

    uint8_t deviceGrantKind = 0;
    bool requestDeviceGrant = false;
    uint64_t deviceGrantParam = 0;
};