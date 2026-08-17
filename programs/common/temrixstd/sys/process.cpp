#include "loader_types.h"
#include "aslr.h"
#include <temrixstd/sys/process.h>
#include <temrixstd/stdio.h>
#include <temrixstd/sys/mman.h>
#include <temrixstd/sys/process.h>

uint32_t spawnBinaryImage(const char *name,
                          const uint8_t *fileBytes,
                          uint64_t entryPoint,
                          const Section *sections, uint32_t numSections,
                          const Relocation *relocs, uint32_t numRelocs,
                          const LoaderConfig &cfg,
                          const Syscall::Process::CopyDescriptor *extraCopies, uint32_t numExtraCopies,
                          const Syscall::Process::RegisterValue *extraRegs, uint32_t numExtraRegs,
                          uint64_t requestedCapabilities,
                          bool requestDeviceGrant,
                          uint8_t deviceGrantKind,
                          uint64_t deviceGrantParam)
{
    uint64_t imageSpan = 0;
    for (uint32_t i = 0; i < numSections; i++) {
        uint64_t end = sections[i].virtualAddress + sections[i].memorySize;
        if (end > imageSpan) imageSpan = end;
    }

    uint64_t bias = 0;
    if (!pickLoadBias(imageSpan, cfg, &bias)) {
        String::Print("[loader] ASLR placement failed for: ");
        String::Print(name);
        String::Print("\n");
        return (uint32_t)-1;
    }

    constexpr uint32_t MAX_TEMP_SECTIONS = 16;
    if (numSections > MAX_TEMP_SECTIONS) return (uint32_t)-1;
    uint64_t stagingBuf[MAX_TEMP_SECTIONS] = {};

    auto cleanupStaging = [&](uint32_t upTo) {
        for (uint32_t k = 0; k < upTo; k++) {
            if (stagingBuf[k]) Syscall::Memory::Unmap(stagingBuf[k], sections[k].memorySize);
        }
    };

    for (uint32_t i = 0; i < numSections; i++) {
        const auto &sec = sections[i];
        if (sec.memorySize == 0) continue;

        uint64_t scratch = Syscall::Memory::Map(sec.memorySize, ::Memory::Read | ::Memory::Write | ::Memory::User);
        if (!scratch) {
            cleanupStaging(i);
            return (uint32_t)-1;
        }

        ::Memory::Set((void *)scratch, 0, sec.memorySize);
        if (sec.fileSize > 0) {
            ::Memory::Copy((void *)scratch, fileBytes + sec.fileOffset, sec.fileSize);
        }
        stagingBuf[i] = scratch;
    }

    for (uint32_t i = 0; i < numRelocs; i++) {
        const auto &r = relocs[i];
        for (uint32_t s = 0; s < numSections; s++) {
            const auto &sec = sections[s];
            if (r.offset >= sec.virtualAddress &&
                r.offset < sec.virtualAddress + sec.memorySize &&
                sec.memorySize - (r.offset - sec.virtualAddress) >= sizeof(uint64_t)) 
            {
                uint64_t localAddr = stagingBuf[s] + (r.offset - sec.virtualAddress);
                *(uint64_t *)localAddr = bias + (uint64_t)r.addend;
                break;
            }
        }
    }

    Syscall::Process::MapDescriptor  maps[MAX_TEMP_SECTIONS + 1];
    Syscall::Process::CopyDescriptor copies[MAX_TEMP_SECTIONS + 4];
    uint32_t numMaps = 0, numCopies = 0;

    for (uint32_t i = 0; i < numSections; i++) {
        const auto &sec = sections[i];
        uint64_t biasedVA = sec.virtualAddress + bias;

        maps[numMaps++] = {biasedVA, (sec.memorySize + 0xFFF) / 0x1000, sec.flags, false};
        if (sec.memorySize > 0) {
            copies[numCopies++] = {stagingBuf[i], biasedVA, sec.memorySize};
        }
    }

    maps[numMaps++] = {cfg.stackTop - cfg.stackSize, cfg.stackSize / 0x1000, ::Memory::Write, false};

    uint64_t lowestExtraDest = cfg.stackTop;
    for (uint32_t i = 0; i < numExtraCopies; i++) {
        copies[numCopies++] = extraCopies[i];
        if (extraCopies[i].dst < lowestExtraDest) lowestExtraDest = extraCopies[i].dst;
    }

    Syscall::Process::RegisterValue regs[8];
    for (uint32_t i = 0; i < numExtraRegs; i++) regs[i] = extraRegs[i];

    uint64_t initialSP = (numExtraCopies > 0) ? (lowestExtraDest - 8) : (cfg.stackTop - 8);

    Syscall::Process::SpawnInfo info{};
    info.maps = maps;
    info.numMaps = numMaps;
    info.copies = copies;
    info.numCopies = numCopies;
    info.registers = regs;
    info.numRegisters = numExtraRegs;
    info.entry = entryPoint + bias;
    info.stackPointer = initialSP;
    info.requestedCapabilities = requestedCapabilities;
    info.requestDeviceGrant = requestDeviceGrant;
    info.deviceGrantKind = deviceGrantKind;
    info.deviceGrantParam = deviceGrantParam;

    uint32_t pid = (uint32_t)Syscall::Process::Spawn(&info);

    cleanupStaging(numSections);
    return pid;
}