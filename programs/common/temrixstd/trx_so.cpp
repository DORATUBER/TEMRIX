#include "trx_so.h"
#include <temrixstd/stdio.h>
#include <temrixstd/sys/mman.h>
#include <temrixstd/stdlib.h>

namespace Trx {

    void* DynamicLibrary::Open(const uint8_t *fileBytes, uint32_t fileSize) {
        if (fileSize < sizeof(TrxSoHeader)) return nullptr;

        const TrxSoHeader *hdr = (const TrxSoHeader *)fileBytes;
        if (hdr->magic[0] != 'T' || hdr->magic[1] != 'R' ||
            hdr->magic[2] != 'S' || hdr->magic[3] != 'O') 
        {
            return nullptr; 
        }

        auto *secTable   = (const TrxSection *)(fileBytes + hdr->sectionTableOffset);
        auto *relocTable = (const TrxReloc *)(fileBytes + hdr->relocTableOffset);

        uint64_t imageSpan = 0;
        for (uint32_t i = 0; i < hdr->numSections; i++) {
            uint64_t end = secTable[i].virtualAddress + secTable[i].memorySize;
            if (end > imageSpan) imageSpan = end;
        }

        if (imageSpan == 0) return nullptr;

        uint64_t bias = Syscall::Memory::Map(imageSpan, ::Memory::Read | ::Memory::Write | ::Memory::User);
        if (!bias) {
            String::Print("[trx_dl] Failed to allocate memory map for shared object\n");
            return nullptr;
        }

        for (uint32_t i = 0; i < hdr->numSections; i++) {
            const auto &sec = secTable[i];
            if (sec.memorySize == 0) continue;

            uint64_t targetAddr = bias + sec.virtualAddress;
            
            ::Memory::Set((void *)targetAddr, 0, sec.memorySize);
            
            if (sec.fileSize > 0) {
                ::Memory::Copy((void *)targetAddr, fileBytes + sec.fileOffset, sec.fileSize);
            }
        }

        for (uint32_t i = 0; i < hdr->numRelocs; i++) {
            const auto &r = relocTable[i];
            uint64_t targetLocation = bias + r.offset;
            *(uint64_t *)targetLocation = bias + (uint64_t)r.addend;
        }

        LibraryHandle *lib = (LibraryHandle *)malloc(sizeof(LibraryHandle));
        if (!lib) {
            Syscall::Memory::Unmap(bias, imageSpan);
            return nullptr;
        }

        lib->baseAddress = bias;
        lib->totalImageSize = imageSpan;
        lib->refCount = 1;

        if (hdr->numSyms > 0 && hdr->symTableOffset > 0) {
            lib->symTable = (const TrxSoSymbol *)(fileBytes + hdr->symTableOffset);
            lib->numSyms  = hdr->numSyms;
            lib->strTable = (const char *)(fileBytes + hdr->strTableOffset);
        } else {
            lib->symTable = nullptr;
            lib->numSyms = 0;
            lib->strTable = nullptr;
        }

        return (void *)lib;
    }

    void* DynamicLibrary::Symbol(void *handle, const char *symbolName) {
        if (!handle || !symbolName) return nullptr;

        auto *lib = (LibraryHandle *)handle;
        if (!lib->symTable || !lib->strTable) return nullptr;

        for (uint32_t i = 0; i < lib->numSyms; i++) {
            const auto &sym = lib->symTable[i];
            const char *name = lib->strTable + sym.nameOffset;

            if (String::Compare(name, symbolName) == 0) {
                return (void *)(lib->baseAddress + sym.value);
            }
        }

        return nullptr;
    }

    int DynamicLibrary::Close(void *handle) {
        if (!handle) return -1;

        LibraryHandle *lib = (LibraryHandle *)handle;
        lib->refCount--;

        if (lib->refCount <= 0) {
            Syscall::Memory::Unmap(lib->baseAddress, lib->totalImageSize);
            free(lib);
        }

        return 0;
    }
}