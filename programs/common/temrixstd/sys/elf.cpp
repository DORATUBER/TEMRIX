#include <temrixstd/sys/process.h>
#include <temrixstd/stdio.h>
#include <temrixstd/sys/mman.h>

#pragma pack(push, 1)

constexpr uint8_t ELF_MAG0 = 0x7F;
constexpr uint8_t ELF_MAG1 = 'E';
constexpr uint8_t ELF_MAG2 = 'L';
constexpr uint8_t ELF_MAG3 = 'F';

constexpr uint16_t ET_EXEC = 2;
constexpr uint16_t ET_DYN  = 3;

constexpr uint32_t PT_LOAD = 1;

constexpr uint32_t PF_X = 0x1;
constexpr uint32_t PF_W = 0x2;
constexpr uint32_t PF_R = 0x4;

struct Elf64Ehdr {
    uint8_t  ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};

struct Elf64Phdr {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
};

#pragma pack(pop)

constexpr uint64_t ELF_STACK_TOP  = 0x7FFFFFFFE000ULL;
constexpr uint64_t ELF_STACK_SIZE = 2 * 1024 * 1024;

constexpr uint64_t ELF_ARG_AREA_SIZE = 0x1000;

constexpr uint64_t ELF_PIE_BASE = 0x555555554000ULL;

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

static uint32_t elfFlagsToTrx(uint32_t pFlags)
{
    uint32_t out = 0;
    if (pFlags & PF_R) out |= ::Memory::Read;
    if (pFlags & PF_W) out |= ::Memory::Write;
    if (pFlags & PF_X) out |= ::Memory::Execute;
    return out;
}

uint32_t spawnElfFromBuffer(const char *path, uint8_t *buf, uint32_t fileSize,
                             int argc, const char *const argv[],
                             uint64_t requestedCapabilities,
                             bool requestDeviceGrant,
                             uint8_t deviceGrantKind,
                             uint64_t deviceGrantParam)
{
    Elf64Ehdr *hdr = (Elf64Ehdr *)buf;
    if (hdr->ident[0] != ELF_MAG0 || hdr->ident[1] != ELF_MAG1 ||
        hdr->ident[2] != ELF_MAG2 || hdr->ident[3] != ELF_MAG3)
    {
        String::Print("[init] bad magic: ");
        String::Print(path);
        String::Print("\n");
        return (uint32_t)-1;
    }

    if (hdr->type != ET_EXEC && hdr->type != ET_DYN)
    {
        String::Print("[init] unsupported elf type: ");
        String::Print(path);
        String::Print("\n");
        return (uint32_t)-1;
    }

    if (hdr->phnum > 7) {
        String::Print("[init] too many program headers: ");
        String::Print(path);
        String::Print("\n");
        return (uint32_t)-1;
    }

    const uint64_t loadBase = (hdr->type == ET_DYN) ? ELF_PIE_BASE : 0;

    auto *phTable = (Elf64Phdr *)(buf + hdr->phoff);

    Syscall::Process::MapDescriptor maps[8];
    Syscall::Process::CopyDescriptor copies[8];
    uint32_t numMaps = 0, numCopies = 0;
    uint64_t highestEnd = 0;

    for (uint16_t i = 0; i < hdr->phnum; i++)
    {
        Elf64Phdr &ph = phTable[i];
        if (ph.type != PT_LOAD) continue;

        uint64_t vaddr = loadBase + ph.vaddr;
        bool needsZeroFill = ph.memsz > ph.filesz;

        maps[numMaps++] = {vaddr, (ph.memsz + 0xFFF) / 0x1000,
                            elfFlagsToTrx(ph.flags), needsZeroFill};

        if (ph.filesz > 0)
        {
            copies[numCopies++] = {(uint64_t)(buf + ph.offset), vaddr, ph.filesz};
        }

        uint64_t end = vaddr + ph.memsz;
        if (end > highestEnd) highestEnd = end;
    }

    maps[numMaps++] = {ELF_STACK_TOP - ELF_STACK_SIZE, ELF_STACK_SIZE / 0x1000, ::Memory::Write, false};

    uint8_t argScratch[ELF_ARG_AREA_SIZE];
    uint64_t argAreaBase = ELF_STACK_TOP - ELF_ARG_AREA_SIZE;

    uint64_t argBlockSize = buildArgBlock(argScratch, ELF_ARG_AREA_SIZE, argAreaBase, argc, argv);
    if (argc > 0 && argBlockSize == 0)
    {
        String::Print("[loader] argv too large: ");
        String::Print(path);
        String::Print("\n");
        return (uint32_t)-1;
    }

    copies[numCopies++] = {(uint64_t)argScratch, argAreaBase, argBlockSize};

    Syscall::Process::RegisterValue regs[2] = {
        {(uint8_t)Syscall::Process::SpawnReg::RDI, (uint64_t)argc},
        {(uint8_t)Syscall::Process::SpawnReg::RSI, argAreaBase},
    };

    Syscall::Process::SpawnInfo info{};
    info.maps = maps;
    info.numMaps = numMaps;
    info.copies = copies;
    info.numCopies = numCopies;
    info.registers = regs;
    info.numRegisters = 2;
    info.entry = loadBase + hdr->entry;
    info.stackPointer = ELF_STACK_TOP - 8;
    info.requestedCapabilities = requestedCapabilities;
    info.requestDeviceGrant = requestDeviceGrant;
    info.deviceGrantKind = deviceGrantKind;
    info.deviceGrantParam = deviceGrantParam;

    uint32_t pid = (uint32_t)Syscall::Process::Spawn(&info);

    char tmp[21];
    if (pid == (uint32_t)-1)
    {
        String::Print("[loader] spawn failed: ");
        String::Print(path);
        String::Print("\n");
    }
    else
    {
        String::Print("[loader] spawned: ");
        String::Print(path);
        String::Print(" pid=");
        String::Print(String::FromU64(pid, tmp));
        String::Print("\n");
    }

    return pid;
}