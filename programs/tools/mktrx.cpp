#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>

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

constexpr uint8_t ELF_MAGIC[4] = {0x7f, 'E', 'L', 'F'};
constexpr uint32_t SEGMENT_TYPE_LOAD = 1;
constexpr uint32_t PF_X = 0x1;
constexpr uint32_t PF_W = 0x2;
constexpr uint32_t PF_R = 0x4;

constexpr uint16_t ET_EXEC = 2;
constexpr uint16_t ET_DYN  = 3;

constexpr uint32_t SHT_RELA = 4;
constexpr uint32_t R_X86_64_RELATIVE = 8;

#pragma pack(push, 1)
struct ElfHeader {
    uint8_t  magic_and_ident[16];
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

struct ProgramSegment {
    uint32_t type;
    uint32_t flags;
    uint64_t file_offset;
    uint64_t virtual_address;
    uint64_t physical_address;
    uint64_t file_size;
    uint64_t memory_size;
    uint64_t alignment;
};

struct SectionHeader {
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

struct Elf64Rela {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
};
#pragma pack(pop)

std::vector<uint8_t> read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open input file " << filename << "\n";
        exit(1);
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "Error: Failed to read file data\n";
        exit(1);
    }
    return buffer;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " input.elf output.trx\n";
        return 1;
    }

    std::vector<uint8_t> file_data = read_file(argv[1]);

    if (file_data.size() < sizeof(ElfHeader)) {
        std::cerr << "Error: File too small to be a valid ELF\n";
        return 1;
    }

    auto* elf_header = reinterpret_cast<ElfHeader*>(file_data.data());
    if (std::memcmp(elf_header->magic_and_ident, ELF_MAGIC, 4) != 0) {
        std::cerr << "Not an ELF file\n";
        return 1;
    }
    if (elf_header->magic_and_ident[4] != 2) {
        std::cerr << "Only 64-bit ELF supported\n";
        return 1;
    }
    if (elf_header->object_file_type != ET_EXEC && elf_header->object_file_type != ET_DYN) {
        std::cerr << "Error: only ET_EXEC or ET_DYN (PIE) supported\n";
        return 1;
    }
    bool isPie = (elf_header->object_file_type == ET_DYN);

    uint64_t ph_offset = elf_header->program_header_offset;
    uint16_t ph_size   = elf_header->program_header_entry_size;
    uint16_t ph_count  = elf_header->program_header_count;

    if (ph_offset + (static_cast<uint64_t>(ph_count) * ph_size) > file_data.size()) {
        std::cerr << "Error: ELF file is corrupted (Program headers out of bounds)\n";
        return 1;
    }

    struct PendingSection {
        uint64_t virtualAddress;
        uint64_t memorySize;
        uint32_t flags;
        const uint8_t* data; 
        uint64_t fileSize;
    };
    std::vector<PendingSection> pending;

    for (int i = 0; i < ph_count; ++i) {
        auto* segment = reinterpret_cast<ProgramSegment*>(
            file_data.data() + ph_offset + (i * ph_size));

        if (segment->type != SEGMENT_TYPE_LOAD) continue;
        if (segment->memory_size == 0) continue;

        if (segment->file_offset + segment->file_size > file_data.size()) {
            std::cerr << "Error: ELF file is corrupted (Segment data out of bounds)\n";
            return 1;
        }
        if (segment->file_size > segment->memory_size) {
            std::cerr << "Error: segment file_size exceeds memory_size\n";
            return 1;
        }

        uint32_t flags = 0;
        if (segment->flags & PF_R) flags |= TRX_SEC_READ;
        if (segment->flags & PF_W) flags |= TRX_SEC_WRITE;
        if (segment->flags & PF_X) flags |= TRX_SEC_EXEC;

        pending.push_back({
            segment->virtual_address,
            segment->memory_size,
            flags,
            file_data.data() + segment->file_offset,
            segment->file_size
        });
    }

    if (pending.size() > 7) {
        std::cerr << "Error: too many LOAD segments (max 7 supported by loader)\n";
        return 1;
    }

    std::vector<TrxReloc> relocs;
    if (isPie) {
        uint64_t sh_off   = elf_header->section_header_offset;
        uint16_t sh_size  = elf_header->section_header_entry_size;
        uint16_t sh_count = elf_header->section_header_count;

        if (sh_off + (uint64_t)sh_count * sh_size > file_data.size()) {
            std::cerr << "Error: section headers out of bounds\n";
            return 1;
        }

        for (int i = 0; i < sh_count; i++) {
            auto *sh = reinterpret_cast<SectionHeader*>(file_data.data() + sh_off + i * sh_size);
            if (sh->type != SHT_RELA) continue;

            if (sh->offset + sh->size > file_data.size() || sh->entsize == 0) {
                std::cerr << "Error: RELA section out of bounds\n";
                return 1;
            }

            uint64_t count = sh->size / sh->entsize;
            for (uint64_t j = 0; j < count; j++) {
                auto *rela = reinterpret_cast<Elf64Rela*>(
                    file_data.data() + sh->offset + j * sh->entsize);

                uint32_t type = (uint32_t)(rela->r_info & 0xFFFFFFFF);
                uint32_t sym  = (uint32_t)(rela->r_info >> 32);

                if (type != R_X86_64_RELATIVE) {
                    std::cerr << "Error: unsupported relocation type " << type
                               << " (only R_X86_64_RELATIVE supported, no dynamic symbols yet)\n";
                    return 1;
                }
                if (sym != 0) {
                    std::cerr << "Error: relative relocation has nonzero symbol index\n";
                    return 1;
                }

                relocs.push_back({rela->r_offset, rela->r_addend});
            }
        }
    }

    TrxHeader trx_header{};
    std::memcpy(trx_header.magic, "TREX", 4);
    trx_header.version = 3;
    trx_header.entry = elf_header->entry_point_address;
    trx_header.numSections = static_cast<uint32_t>(pending.size());
    trx_header.sectionTableOffset = sizeof(TrxHeader);
    trx_header.relocTableOffset = sizeof(TrxHeader) + static_cast<uint32_t>(pending.size() * sizeof(TrxSection));
    trx_header.numRelocs = static_cast<uint32_t>(relocs.size());

    std::vector<TrxSection> sections;
    uint64_t dataCursor = trx_header.relocTableOffset + relocs.size() * sizeof(TrxReloc);
    for (auto& p : pending) {
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
    if (!out_file.is_open()) {
        std::cerr << "Error: Could not open output file " << argv[2] << "\n";
        return 1;
    }

    out_file.write(reinterpret_cast<const char*>(&trx_header), sizeof(trx_header));
    out_file.write(reinterpret_cast<const char*>(sections.data()),
                    sections.size() * sizeof(TrxSection));
    out_file.write(reinterpret_cast<const char*>(relocs.data()),
                    relocs.size() * sizeof(TrxReloc));
    for (auto& p : pending) {
        if (p.fileSize) out_file.write(reinterpret_cast<const char*>(p.data), p.fileSize);
    }

    std::cout << "Created " << argv[2] << "\n";
    std::cout << "  type:     " << (isPie ? "PIE (ET_DYN)" : "static (ET_EXEC)") << "\n";
    std::cout << "  entry:    0x" << std::hex << trx_header.entry << std::dec << "\n";
    std::cout << "  sections: " << pending.size() << "\n";
    std::cout << "  relocs:   " << relocs.size() << "\n";
    for (size_t i = 0; i < pending.size(); ++i) {
        std::cout << "    [" << i << "] virtualAddress=0x" << std::hex << pending[i].virtualAddress
                   << std::dec << " fileSize=" << pending[i].fileSize
                   << " memorySize=" << pending[i].memorySize
                   << " flags=" << (pending[i].flags & TRX_SEC_READ ? "R" : "-")
                   << (pending[i].flags & TRX_SEC_WRITE ? "W" : "-")
                   << (pending[i].flags & TRX_SEC_EXEC ? "X" : "-") << "\n";
    }

    return 0;
}