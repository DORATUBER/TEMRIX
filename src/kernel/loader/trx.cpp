#include "trx.hpp"
#include "KernelState.hpp"
#include "Serial.hpp" 
#include "entropy.hpp"

#define TRX_DEBUG 1

#if TRX_DEBUG
#define TRX_LOG(...) Serial::printf(__VA_ARGS__)
#else
#define TRX_LOG(...) do {} while (0)
#endif

constexpr uint64_t TRX_PIE_BASE_MIN  = 0x10000000ULL;
constexpr uint64_t TRX_PIE_BASE_MAX  = 0x7F0000000000ULL;
constexpr uint64_t TRX_PIE_ALIGN     = 0x200000ULL;
constexpr uint64_t TRX_PIE_GUARD_GAP = 0x10000000ULL;

constexpr uint32_t TRX_MAX_SECTIONS = 8; 

static bool pickLoadBias(uint64_t imageSpan, uint64_t *outBias)
{
    if (imageSpan + TRX_PIE_GUARD_GAP < imageSpan ||
        TRX_PIE_BASE_MIN + imageSpan + TRX_PIE_GUARD_GAP > TRX_PIE_BASE_MAX)
    {
        TRX_LOG("[trx] pickLoadBias: imageSpan=0x%llx does not fit in PIE window\n", imageSpan);
        return false;
    }

    uint64_t usableRange = TRX_PIE_BASE_MAX - TRX_PIE_BASE_MIN - imageSpan - TRX_PIE_GUARD_GAP;
    uint64_t numSlots = usableRange / TRX_PIE_ALIGN;
    if (numSlots == 0)
        return false;

    uint64_t slot = getRandom64() % numSlots;
    *outBias = TRX_PIE_BASE_MIN + slot * TRX_PIE_ALIGN;

    TRX_LOG("[trx] pickLoadBias: imageSpan=0x%llx numSlots=%llu slot=%llu bias=0x%llx\n",
            imageSpan, numSlots, slot, *outBias);

    return true;
}

static const char *secFlagsStr(uint32_t secFlags)
{
    static char buf[4];
    buf[0] = (secFlags & TRX_SEC_READ)  ? 'R' : '-';
    buf[1] = (secFlags & TRX_SEC_WRITE) ? 'W' : '-';
    buf[2] = (secFlags & TRX_SEC_EXEC)  ? 'X' : '-';
    buf[3] = '\0';
    return buf;
}

static bool validateTrxLayout(const TrxHeader *hdr, uint64_t size,
                               const TrxSection *secTable, const TrxReloc *relocTable)
{
    TRX_LOG("[trx] validate: size=%llu numSections=%u numRelocs=%u sectionTableOffset=0x%x relocTableOffset=0x%x\n",
            size, hdr->numSections, hdr->numRelocs, hdr->sectionTableOffset, hdr->relocTableOffset);

    if (hdr->numSections > TRX_MAX_SECTIONS)
    {
        TRX_LOG("[trx] validate: too many sections (%u)\n", hdr->numSections);
        return false;
    }

    const uint64_t sectionTableBytes = (uint64_t)hdr->numSections * sizeof(TrxSection);
    if (hdr->sectionTableOffset > size ||
        sectionTableBytes > size - hdr->sectionTableOffset)
    {
        TRX_LOG("[trx] validate: section table out of bounds\n");
        return false;
    }

    const uint64_t relocTableBytes = (uint64_t)hdr->numRelocs * sizeof(TrxReloc);
    if (hdr->relocTableOffset > size ||
        relocTableBytes > size - hdr->relocTableOffset)
    {
        TRX_LOG("[trx] validate: reloc table out of bounds\n");
        return false;
    }

    for (uint32_t i = 0; i < hdr->numSections; i++)
    {
        const TrxSection &sec = secTable[i];

        TRX_LOG("[trx]   section[%u] vaddr=0x%llx fileOffset=0x%llx fileSize=0x%llx memSize=0x%llx flags=%s\n",
                i, sec.vaddr, sec.fileOffset, sec.fileSize, sec.memSize, secFlagsStr(sec.flags));

        if (sec.fileSize > sec.memSize)
        {
            TRX_LOG("[trx] validate: section[%u] fileSize > memSize\n", i);
            return false;
        }

        if (sec.fileOffset > size ||
            sec.fileSize > size - sec.fileOffset)
        {
            TRX_LOG("[trx] validate: section[%u] data out of bounds\n", i);
            return false;
        }

        if (sec.vaddr + sec.memSize < sec.vaddr)
        {
            TRX_LOG("[trx] validate: section[%u] VA range overflows\n", i);
            return false;
        }
    }

    for (uint32_t i = 0; i < hdr->numSections; i++)
    {
        const uint64_t aStart = secTable[i].vaddr;
        const uint64_t aEnd   = aStart + secTable[i].memSize;

        for (uint32_t j = i + 1; j < hdr->numSections; j++)
        {
            const uint64_t bStart = secTable[j].vaddr;
            const uint64_t bEnd   = bStart + secTable[j].memSize;

            if (aStart < bEnd && bStart < aEnd)
            {
                TRX_LOG("[trx] validate: sections %u and %u overlap\n", i, j);
                return false;
            }
        }
    }

    for (uint32_t i = 0; i < hdr->numRelocs; i++)
    {
        const TrxReloc &r = relocTable[i];
        bool found = false;

        for (uint32_t s = 0; s < hdr->numSections; s++)
        {
            const TrxSection &sec = secTable[s];
            if (r.offset >= sec.vaddr &&
                r.offset < sec.vaddr + sec.memSize &&
                sec.memSize - (r.offset - sec.vaddr) >= sizeof(uint64_t))
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            TRX_LOG("[trx] validate: reloc[%u] target outside any section\n", i);
            return false;
        }
    }

    TRX_LOG("[trx] validate: OK\n");
    return true;
}

bool TRX::parse(const uint8_t* binary, uint64_t size, KernelSpawnInfo* out) {
    TRX_LOG("[trx] parse: size=%llu\n", size);

    if (size < sizeof(TrxHeader)) {
        TRX_LOG("[trx] parse: buffer too small for header\n");
        return false;
    }

    auto* hdr = (const TrxHeader*)binary;
    if (hdr->magic[0] != 'T' || hdr->magic[1] != 'R' ||
        hdr->magic[2] != 'E' || hdr->magic[3] != 'X') {
        TRX_LOG("[trx] parse: bad magic\n");
        return false;
    }

    TRX_LOG("[trx] header: version=%u entry=0x%llx numSections=%u numRelocs=%u\n",
            hdr->version, hdr->entry, hdr->numSections, hdr->numRelocs);

    auto* secTable   = (const TrxSection*)(binary + hdr->sectionTableOffset);
    auto* relocTable = (const TrxReloc*)(binary + hdr->relocTableOffset);

    if (!validateTrxLayout(hdr, size, secTable, relocTable))
        return false;

    uint64_t imageSpan = 0;
    for (uint32_t i = 0; i < hdr->numSections; i++) {
        uint64_t end = secTable[i].vaddr + secTable[i].memSize;
        if (end > imageSpan) imageSpan = end;
    }
    TRX_LOG("[trx] imageSpan=0x%llx\n", imageSpan);

    uint64_t bias;
    if (!pickLoadBias(imageSpan, &bias)) {
        TRX_LOG("[trx] parse: image too large to place with ASLR\n");
        return false;
    }

    out->entry = hdr->entry + bias;
    out->stackSize = 64 * 1024;
    out->bias = bias;
    out->numSections = 0;
    out->relocs = relocTable;
    out->numRelocs = hdr->numRelocs;

    for (uint32_t i = 0; i < hdr->numSections; i++) {
        const TrxSection& sec = secTable[i];

        uint64_t vmFlags = 0;
        if (sec.flags & TRX_SEC_READ)  vmFlags |= Memory::VM_READ;
        if (sec.flags & TRX_SEC_WRITE) vmFlags |= Memory::VM_WRITE;
        if (sec.flags & TRX_SEC_EXEC)  vmFlags |= Memory::VM_EXEC;

        auto& s = out->sections[out->numSections++];
        s.src      = sec.fileSize ? (binary + sec.fileOffset) : nullptr;
        s.dst      = sec.vaddr + bias;
        s.fileSize = sec.fileSize;
        s.memSize  = sec.memSize;
        s.vmFlags  = vmFlags;

        TRX_LOG("[trx]   -> KernelSection[%u] dst=0x%llx memSize=0x%llx vmFlags=0x%llx\n",
                out->numSections - 1, s.dst, s.memSize, s.vmFlags);
    }

    TRX_LOG("[trx] parse: OK entry=0x%llx bias=0x%llx numSections=%u numRelocs=%u\n",
            out->entry, out->bias, out->numSections, out->numRelocs);

    return true;
}

bool TRX::load(const uint8_t* binary, uint64_t size, Task** outTask) {
    TRX_LOG("[trx] load: size=%llu\n", size);

    KernelSpawnInfo info;
    if (!TRX::parse(binary, size, &info)) {
        TRX_LOG("[trx] load: parse failed\n");
        return false;
    }

    bool ok = Loader::spawn(&info, outTask);
    TRX_LOG("[trx] load: Loader::spawn -> %s\n", ok ? "ok" : "failed");
    
    return ok;
}