#include "loader_types.h"
#include <temrixstd/sys/process.h>
#include <temrixstd/stdio.h>
#include <temrixstd/sys/mman.h>
#include <temrixstd/sys/process.h>

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
    uint64_t virtualAddress;
    uint64_t fileOffset;
    uint64_t fileSize;
    uint64_t memorySize;
    uint32_t flags;
    uint32_t reserved;
};

struct TrxReloc {
    uint64_t offset;
    int64_t  addend;
};
#pragma pack(pop)

uint32_t spawnFromBuffer(const char *path, uint8_t *buf, uint32_t fileSize,
                          const Syscall::Process::CopyDescriptor *extraCopies, uint32_t numExtraCopies,
                          const Syscall::Process::RegisterValue *extraRegs, uint32_t numExtraRegs,
                          uint64_t requestedCapabilities,
                          bool requestDeviceGrant,
                          uint8_t deviceGrantKind,
                          uint64_t deviceGrantParam)
{
    if (fileSize < sizeof(TrxHeader)) return (uint32_t)-1;

    TrxHeader *hdr = (TrxHeader *)buf;
    if (hdr->magic[0] != 'T' || hdr->magic[1] != 'R' ||
        hdr->magic[2] != 'E' || hdr->magic[3] != 'X')
    {
        return (uint32_t)-1;
    }

    auto *secTable   = (TrxSection *)(buf + hdr->sectionTableOffset);
    auto *relocTable = (TrxReloc *)(buf + hdr->relocTableOffset);

    Section sections[16];
    for (uint32_t i = 0; i < hdr->numSections && i < 16; i++) {
        sections[i].virtualAddress = secTable[i].virtualAddress;
        sections[i].fileOffset = secTable[i].fileOffset;
        sections[i].fileSize = secTable[i].fileSize;
        sections[i].memorySize = secTable[i].memorySize;
        
        uint32_t memFlags = 0;
        if (secTable[i].flags & TRX_SEC_READ)  memFlags |= ::Memory::Read;
        if (secTable[i].flags & TRX_SEC_WRITE) memFlags |= ::Memory::Write;
        if (secTable[i].flags & TRX_SEC_EXEC)  memFlags |= ::Memory::Execute;
        sections[i].flags = memFlags;
    }

    Relocation relocs[64];
    for (uint32_t i = 0; i < hdr->numRelocs && i < 64; i++) {
        relocs[i].offset = relocTable[i].offset;
        relocs[i].addend = relocTable[i].addend;
    }

    LoaderConfig cfg{
        .baseMin = 0x10000000ULL,
        .baseMax = 0x7F0000000000ULL,
        .alignment = 0x200000ULL,
        .guardGap = 0x10000000ULL,
        .stackTop = 0x7FFFFFFFE000ULL,
        .stackSize = 2 * 1024 * 1024,
        .debug = true
    };

    uint32_t pid = spawnBinaryImage(path, buf, hdr->entry,
                                     sections, hdr->numSections,
                                     relocs, hdr->numRelocs,
                                     cfg,
                                     extraCopies, numExtraCopies,
                                     extraRegs, numExtraRegs,
                                     requestedCapabilities,
                                     requestDeviceGrant,
                                     deviceGrantKind,
                                     deviceGrantParam);

    char tmp[21];
    if (pid == (uint32_t)-1) {
        String::Print("[loader] spawn failed: ");
        String::Print(path);
        String::Print("\n");
    } else {
        String::Print("[loader] spawned: ");
        String::Print(path);
        String::Print(" pid=");
        String::Print(String::FromU64(pid, tmp));
        String::Print("\n");
    }

    return pid;
}

constexpr uint64_t TRX_STACK_TOP     = 0x7FFFFFFFE000ULL;
constexpr uint64_t TRX_ARG_AREA_SIZE = 0x1000;

static uint64_t buildArgBlock(uint8_t *out, uint64_t outSize, uint64_t destBaseVA,
                               int argc, const char *const argv[])
{
    if (argc < 0) argc = 0;

    const uint64_t ptrBytes = 8 * ((uint64_t)argc + 1);

    uint64_t strBytes = 0;
    for (int i = 0; i < argc; i++)
        strBytes += String::Length(argv[i]) + 1;

    const uint64_t total = ptrBytes + strBytes;
    if (total > outSize) return 0;

    uint64_t *argvSlots = (uint64_t *)out;
    uint64_t strOff = ptrBytes;

    for (int i = 0; i < argc; i++)
    {
        uint64_t len = String::Length(argv[i]) + 1;
        ::Memory::Copy(out + strOff, argv[i], len);
        argvSlots[i] = destBaseVA + strOff;
        strOff += len;
    }
    argvSlots[argc] = 0;

    return total;
}

uint32_t spawnFromBufferWithArgs(const char *path, uint8_t *buf, uint32_t fileSize,
                                  int argc, const char *const argv[],
                                  uint64_t requestedCapabilities,
                                  bool requestDeviceGrant,
                                  uint8_t deviceGrantKind,
                                  uint64_t deviceGrantParam)
{
    uint8_t argScratch[TRX_ARG_AREA_SIZE];
    uint64_t argAreaBase = TRX_STACK_TOP - TRX_ARG_AREA_SIZE;

    uint64_t argBlockSize = buildArgBlock(argScratch, TRX_ARG_AREA_SIZE, argAreaBase, argc, argv);
    if (argc > 0 && argBlockSize == 0)
    {
        String::Print("[loader] argv too large: ");
        String::Print(path);
        String::Print("\n");
        return (uint32_t)-1;
    }

    Syscall::Process::CopyDescriptor argCopy = {(uint64_t)argScratch, argAreaBase, argBlockSize};

    Syscall::Process::RegisterValue regs[2] = {
        {(uint8_t)Syscall::Process::SpawnReg::RDI, (uint64_t)argc},
        {(uint8_t)Syscall::Process::SpawnReg::RSI, argAreaBase},
    };

    return spawnFromBuffer(path, buf, fileSize,
                            &argCopy, 1,
                            regs, 2,
                            requestedCapabilities,
                            requestDeviceGrant,
                            deviceGrantKind,
                            deviceGrantParam);
}