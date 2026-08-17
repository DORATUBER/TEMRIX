#pragma once
#include <temrixstd/stdint.h>  

namespace Trx
{
#pragma pack(push, 1)

    constexpr uint32_t TRX_SEC_READ = 0x1;
    constexpr uint32_t TRX_SEC_WRITE = 0x2;
    constexpr uint32_t TRX_SEC_EXEC = 0x4;

    struct TrxSoHeader
    {
        char magic[4]; 
        uint32_t version;
        uint32_t numSections;
        uint32_t sectionTableOffset;
        uint32_t relocTableOffset;
        uint32_t numRelocs;
        uint32_t symTableOffset;
        uint32_t numSyms;
        uint32_t strTableOffset;
        uint32_t strTableSize;
    };

    struct TrxSection
    {
        uint64_t virtualAddress;
        uint64_t fileOffset;
        uint64_t fileSize;
        uint64_t memorySize;
        uint32_t flags;
        uint32_t reserved;
    };

    struct TrxReloc
    {
        uint64_t offset;
        int64_t addend;
    };

    struct TrxSoSymbol
    {
        uint32_t nameOffset;
        uint64_t value;
        uint64_t size;
        uint32_t flags;
    };

#pragma pack(pop)

    struct LibraryHandle
    {
        uint64_t baseAddress;
        uint64_t totalImageSize;

        const TrxSoSymbol *symTable;
        uint32_t numSyms;
        const char *strTable;

        int refCount;
    };

    class DynamicLibrary
    {
    public:
        static void *Open(const uint8_t *fileBytes, uint32_t fileSize);
        static void *Symbol(void *handle, const char *symbolName);
        static int Close(void *handle);
    };
}