#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>

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

constexpr uint8_t ELF_MAGIC[4] = {0x7f, 'E', 'L', 'F'};
constexpr uint32_t SEGMENT_TYPE_LOAD = 1;
constexpr uint32_t PF_X = 0x1;
constexpr uint32_t PF_W = 0x2;
constexpr uint32_t PF_R = 0x4;

constexpr uint16_t ET_DYN = 3;

constexpr uint32_t SHT_DYNSYM = 11;
constexpr uint32_t SHT_RELA = 4;

constexpr uint32_t R_X86_64_RELATIVE = 8;
constexpr uint32_t R_X86_64_GLOB_DAT = 6;
constexpr uint32_t R_X86_64_JUMP_SLOT = 7;
constexpr uint32_t R_X86_64_64 = 1;

constexpr uint16_t SHN_UNDEF = 0;
constexpr uint8_t STB_LOCAL = 0;

#pragma pack(push, 1)
struct ElfHeader
{
    uint8_t magic_and_ident[16];
    uint16_t object_file_type;
    uint16_t architecture;
    uint32_t object_file_version;
    uint64_t entry_point_address;
    uint64_t program_header_offset;
    uint64_t section_header_offset;
    uint32_t processor_flags;
    uint16_t elf_header_size;
    uint16_t program_header_entry_size;
    uint16_t program_header_count;
    uint16_t section_header_entry_size;
    uint16_t section_header_count;
    uint16_t section_header_string_table_index;
};

struct ProgramSegment
{
    uint32_t type;
    uint32_t flags;
    uint64_t file_offset;
    uint64_t virtual_address;
    uint64_t physical_address;
    uint64_t file_size;
    uint64_t memory_size;
    uint64_t alignment;
};

struct SectionHeader
{
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t addr;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t addralign;
    uint64_t entsize;
};

struct Elf64Sym
{
    uint32_t name;
    uint8_t info;
    uint8_t other;
    uint16_t shndx;
    uint64_t value;
    uint64_t size;
};

struct Elf64Rela
{
    uint64_t r_offset;
    uint64_t r_info;
    int64_t r_addend;
};
#pragma pack(pop)

std::vector<uint8_t> read_file(const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open input file " << filename << "\n";
        exit(1);
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char *>(buffer.data()), size))
    {
        std::cerr << "Error: Failed to read file data\n";
        exit(1);
    }
    return buffer;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " input.so output.trso\n";
        return 1;
    }

    std::vector<uint8_t> file_data = read_file(argv[1]);

    if (file_data.size() < sizeof(ElfHeader))
    {
        std::cerr << "Error: File too small to be a valid ELF\n";
        return 1;
    }

    auto *elf_header = reinterpret_cast<ElfHeader *>(file_data.data());
    if (std::memcmp(elf_header->magic_and_ident, ELF_MAGIC, 4) != 0)
    {
        std::cerr << "Not an ELF file\n";
        return 1;
    }
    if (elf_header->magic_and_ident[4] != 2)
    {
        std::cerr << "Only 64-bit ELF supported\n";
        return 1;
    }
    if (elf_header->object_file_type != ET_DYN)
    {
        std::cerr << "Error: only ET_DYN (shared object) input supported. "
                     "Build with -shared / -fPIC.\n";
        return 1;
    }

    uint64_t ph_offset = elf_header->program_header_offset;
    uint16_t ph_size = elf_header->program_header_entry_size;
    uint16_t ph_count = elf_header->program_header_count;

    if (ph_offset + (static_cast<uint64_t>(ph_count) * ph_size) > file_data.size())
    {
        std::cerr << "Error: ELF file is corrupted (Program headers out of bounds)\n";
        return 1;
    }

    
    struct PendingSection
    {
        uint64_t virtualAddress;
        uint64_t memorySize;
        uint32_t flags;
        const uint8_t *data;
        uint64_t fileSize;
    };
    std::vector<PendingSection> pending;

    for (int i = 0; i < ph_count; ++i)
    {
        auto *segment = reinterpret_cast<ProgramSegment *>(
            file_data.data() + ph_offset + (i * ph_size));

        if (segment->type != SEGMENT_TYPE_LOAD)
            continue;
        if (segment->memory_size == 0)
            continue;

        if (segment->file_offset + segment->file_size > file_data.size())
        {
            std::cerr << "Error: ELF file is corrupted (Segment data out of bounds)\n";
            return 1;
        }
        if (segment->file_size > segment->memory_size)
        {
            std::cerr << "Error: segment file_size exceeds memory_size\n";
            return 1;
        }

        uint32_t flags = 0;
        if (segment->flags & PF_R)
            flags |= TRX_SEC_READ;
        if (segment->flags & PF_W)
            flags |= TRX_SEC_WRITE;
        if (segment->flags & PF_X)
            flags |= TRX_SEC_EXEC;

        pending.push_back({segment->virtual_address,
                           segment->memory_size,
                           flags,
                           file_data.data() + segment->file_offset,
                           segment->file_size});
    }

    if (pending.empty())
    {
        std::cerr << "Error: no LOAD segments found\n";
        return 1;
    }

    uint64_t sh_off = elf_header->section_header_offset;
    uint16_t sh_size = elf_header->section_header_entry_size;
    uint16_t sh_count = elf_header->section_header_count;

    if (sh_off + (uint64_t)sh_count * sh_size > file_data.size())
    {
        std::cerr << "Error: section headers out of bounds\n";
        return 1;
    }

    const SectionHeader *dynsymSec = nullptr;
    const SectionHeader *dynstrSec = nullptr;
    std::vector<TrxReloc> relocs;
    uint32_t skippedRelocs = 0;

    for (int i = 0; i < sh_count; i++)
    {
        auto *sh = reinterpret_cast<SectionHeader *>(file_data.data() + sh_off + i * sh_size);

        if (sh->type == SHT_DYNSYM)
        {
            dynsymSec = sh;
            if (sh->link < sh_count)
            {
                dynstrSec = reinterpret_cast<SectionHeader *>(
                    file_data.data() + sh_off + sh->link * sh_size);
            }
        }

        if (sh->type == SHT_RELA)
        {
            if (sh->offset + sh->size > file_data.size() || sh->entsize == 0)
            {
                std::cerr << "Error: RELA section out of bounds\n";
                return 1;
            }
            uint64_t count = sh->size / sh->entsize;
            for (uint64_t j = 0; j < count; j++)
            {
                auto *rela = reinterpret_cast<Elf64Rela *>(
                    file_data.data() + sh->offset + j * sh->entsize);

                uint32_t type = (uint32_t)(rela->r_info & 0xFFFFFFFF);
                uint32_t sym = (uint32_t)(rela->r_info >> 32);

                if (type == R_X86_64_RELATIVE && sym == 0)
                {
                    relocs.push_back({rela->r_offset, rela->r_addend});
                }
                else
                {
                    skippedRelocs++;
                }
            }
        }
    }

    if (skippedRelocs > 0)
    {
        std::cerr << "Warning: skipped " << skippedRelocs
                  << " relocation(s) that require symbol resolution "
                     "(GLOB_DAT/JUMP_SLOT/direct). The loader only applies "
                     "R_X86_64_RELATIVE. If the .so calls into other libraries "
                     "or uses non-hidden global data across TUs, this image may "
                     "be broken at runtime.\n";
    }

    std::vector<TrxSoSymbol> symbols;
    std::vector<uint8_t> strTable;
    strTable.push_back('\0'); 

    if (dynsymSec && dynstrSec)
    {
        if (dynsymSec->offset + dynsymSec->size > file_data.size() ||
            dynsymSec->entsize == 0)
        {
            std::cerr << "Error: .dynsym out of bounds\n";
            return 1;
        }
        if (dynstrSec->offset + dynstrSec->size > file_data.size())
        {
            std::cerr << "Error: .dynstr out of bounds\n";
            return 1;
        }

        const char *dynstr = reinterpret_cast<const char *>(
            file_data.data() + dynstrSec->offset);
        uint64_t symCount = dynsymSec->size / dynsymSec->entsize;

        for (uint64_t i = 0; i < symCount; i++)
        {
            auto *sym = reinterpret_cast<Elf64Sym *>(
                file_data.data() + dynsymSec->offset + i * dynsymSec->entsize);

            uint8_t bind = sym->info >> 4;
            if (sym->shndx == SHN_UNDEF)
                continue; 
            if (bind == STB_LOCAL)
                continue; 
            if (sym->name == 0)
                continue; 

            if (dynstrSec->offset + sym->name >= file_data.size())
            {
                std::cerr << "Error: symbol name offset out of bounds\n";
                return 1;
            }
            const char *name = dynstr + sym->name;

            TrxSoSymbol out{};
            out.nameOffset = (uint32_t)strTable.size();
            out.value = sym->value; 
            out.size = sym->size;
            out.flags = 0;
            symbols.push_back(out);

            size_t nameLen = std::strlen(name);
            strTable.insert(strTable.end(), name, name + nameLen);
            strTable.push_back('\0');
        }
    }
    else
    {
        std::cerr << "Warning: no .dynsym found, output will have no exported symbols\n";
    }

    TrxSoHeader trxso_header{};
    std::memcpy(trxso_header.magic, "TRSO", 4);
    trxso_header.version = 1;
    trxso_header.numSections = static_cast<uint32_t>(pending.size());
    trxso_header.sectionTableOffset = sizeof(TrxSoHeader);
    trxso_header.relocTableOffset = trxso_header.sectionTableOffset +
                                    static_cast<uint32_t>(pending.size() * sizeof(TrxSection));
    trxso_header.numRelocs = static_cast<uint32_t>(relocs.size());
    trxso_header.symTableOffset = trxso_header.relocTableOffset +
                                  static_cast<uint32_t>(relocs.size() * sizeof(TrxReloc));
    trxso_header.numSyms = static_cast<uint32_t>(symbols.size());
    trxso_header.strTableOffset = trxso_header.symTableOffset +
                                  static_cast<uint32_t>(symbols.size() * sizeof(TrxSoSymbol));
    trxso_header.strTableSize = static_cast<uint32_t>(strTable.size());

    std::vector<TrxSection> sections;
    uint64_t dataCursor = trxso_header.strTableOffset + strTable.size();
    for (auto &p : pending)
    {
        TrxSection s{};
        s.virtualAddress = p.virtualAddress;
        s.fileOffset = p.fileSize ? dataCursor : 0;
        s.fileSize = p.fileSize;
        s.memorySize = p.memorySize;
        s.flags = p.flags;
        sections.push_back(s);
        dataCursor += p.fileSize;
    }

    std::ofstream out_file(argv[2], std::ios::binary);
    if (!out_file.is_open())
    {
        std::cerr << "Error: Could not open output file " << argv[2] << "\n";
        return 1;
    }

    out_file.write(reinterpret_cast<const char *>(&trxso_header), sizeof(trxso_header));
    out_file.write(reinterpret_cast<const char *>(sections.data()),
                   sections.size() * sizeof(TrxSection));
    out_file.write(reinterpret_cast<const char *>(relocs.data()),
                   relocs.size() * sizeof(TrxReloc));
    out_file.write(reinterpret_cast<const char *>(symbols.data()),
                   symbols.size() * sizeof(TrxSoSymbol));
    out_file.write(reinterpret_cast<const char *>(strTable.data()), strTable.size());
    for (auto &p : pending)
    {
        if (p.fileSize)
            out_file.write(reinterpret_cast<const char *>(p.data), p.fileSize);
    }

    std::cout << "Created " << argv[2] << "\n";
    std::cout << "  sections: " << pending.size() << "\n";
    std::cout << "  relocs:   " << relocs.size()
              << " applied, " << skippedRelocs << " skipped\n";
    std::cout << "  exports:  " << symbols.size() << "\n";
    for (auto &s : symbols)
    {
        std::cout << "    " << (dynstrSec ? (const char *)(strTable.data() + s.nameOffset) : "?")
                  << " @ 0x" << std::hex << s.value << std::dec
                  << " size=" << s.size << "\n";
    }

    return 0;
}