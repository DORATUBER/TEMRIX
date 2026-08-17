#pragma once

#include <temrixstd.h>
#include "Extent.hpp"
#include "FreeSpaceIndex.hpp"
#include "BlockGroup.hpp"

class ExtentAllocator
{
public:
    void init(uint32_t sectorSize, IMetadataBlockIo *io = nullptr)
    {
        this->sectorSize = sectorSize;
        this->blockIo    = io;
    }

    void setBlockIo(IMetadataBlockIo *io) { blockIo = io; }

    static bool isValidHeader(const ExtentTableHeader &hdr)
    {
        return hdr.magic == TEMRIXFS_EXTENT_MAGIC;
    }

    void formatFresh(uint64_t headerLba, uint64_t firstFreeBlock, uint64_t lastFreeBlock)
    {
        freeSpace.reset();

        table.magic          = TEMRIXFS_EXTENT_MAGIC;
        table.version         = 2;
        table.extentCount     = 0;
        table.extentCapacity  = capacityForSectors(1);
        table.reserved        = 0;
        table.metadataSectors = 1;

        table.metadata.direct[0]  = Extent{headerLba + 1, 1};
        table.metadata.directCount = 1;
        table.metadata.reserved0   = 0;
        table.metadata.l1Lba       = 0;
        table.metadata.l2Lba       = 0;
        table.metadata.l3Lba       = 0;

        freeSpace.insert(Extent{firstFreeBlock, (lastFreeBlock - firstFreeBlock) + 1});
        table.extentCount = freeSpace.count();

        String::Printf("[extalloc] formatted fresh table\n");
    }

    void loadHeader(const ExtentTableHeader &hdr)
    {
        table = hdr;
        String::Printf("[extalloc] loaded table header, %u/%u extents, %llu metadata sectors\n",
                        table.extentCount, table.extentCapacity, table.metadataSectors);
    }

    void loadEntries(const uint8_t *buf, uint32_t bufBytes)
    {
        uint32_t count = table.extentCount;

        if (count > TEMRIXFS_MAX_FREE_EXTENTS)
            count = TEMRIXFS_MAX_FREE_EXTENTS;

        uint32_t maxFromBuf = bufBytes / sizeof(Extent);
        if (count > maxFromBuf)
            count = maxFromBuf;

        freeSpace.reset();

        const Extent *arr = (const Extent *)buf;
        for (uint32_t i = 0; i < count; i++)
        {
            if (arr[i].blockCount == 0)
                continue;

            if (freeSpace.insert(arr[i]) == RBTREE_NIL)
            {
                String::Printf("[extalloc] WARNING: node pool exhausted while loading entries\n");
                break;
            }
        }

        table.extentCount = freeSpace.count();
    }

    Extent allocate(uint64_t requestedBlocks)
    {
        Extent failure{0, 0};
        if (requestedBlocks == 0)
            return failure;

        int32_t idx = freeSpace.findBestFit(requestedBlocks);
        if (idx == RBTREE_NIL)
        {
            String::Printf("[extalloc] allocate(%llu) FAILED: no fit\n", requestedBlocks);
            return failure;
        }

        Extent chosen    = freeSpace.extentAt(idx);
        Extent allocated{chosen.startBlock, requestedBlocks};

        if (chosen.blockCount == requestedBlocks)
            freeSpace.remove(idx);
        else
            freeSpace.resize(idx, Extent{chosen.startBlock + requestedBlocks, chosen.blockCount - requestedBlocks});

        table.extentCount = freeSpace.count();

        String::Printf("[extalloc] allocate(%llu) -> [%llu-%llu]\n",
                        requestedBlocks, allocated.startBlock, allocated.startBlock + allocated.blockCount - 1);
        return allocated;
    }

    Extent allocateInGroup(uint64_t requestedBlocks, const BlockGroupDescriptor *groups,
                            uint32_t groupCount, uint32_t preferredGroup)
    {
        if (requestedBlocks == 0 || groupCount == 0)
            return Extent{0, 0};

        for (uint32_t gi = 0; gi < groupCount; gi++)
        {
            uint32_t g = (preferredGroup + gi) % groupCount;
            uint64_t rangeStart = groups[g].startBlock;
            uint64_t rangeEnd   = groups[g].startBlock + groups[g].blockCount - 1;

            RangeCandidate chosen;
            if (!findCandidateInRange(requestedBlocks, rangeStart, rangeEnd, /*allowPartial=*/false, chosen))
                continue;

            Extent allocated = takeFromChosen(chosen, requestedBlocks);
            if (allocated.valid())
            {
                String::Printf("[extalloc] allocateInGroup(%llu, prefer=%u) -> group %u [%llu-%llu]\n",
                                requestedBlocks, preferredGroup, g,
                                allocated.startBlock, allocated.startBlock + allocated.blockCount - 1);
                return allocated;
            }
        }

        String::Printf("[extalloc] allocateInGroup(%llu, prefer=%u) FAILED: no group had room\n",
                        requestedBlocks, preferredGroup);
        return Extent{0, 0};
    }

    uint64_t freeBlocksInRange(uint64_t rangeStart, uint64_t rangeEnd) const
    {
        Extent all[TEMRIXFS_MAX_FREE_EXTENTS];
        uint32_t n = freeSpace.enumerate(all, TEMRIXFS_MAX_FREE_EXTENTS);

        uint64_t total = 0;
        for (uint32_t i = 0; i < n; i++)
        {
            uint64_t s = all[i].startBlock;
            uint64_t e = s + all[i].blockCount - 1;

            uint64_t iStart = (s > rangeStart) ? s : rangeStart;
            uint64_t iEnd   = (e < rangeEnd) ? e : rangeEnd;

            if (iStart <= iEnd)
                total += (iEnd - iStart + 1);
        }
        return total;
    }

    void free(Extent freed)
    {
        if (!freed.valid())
            return;

        String::Printf("[extalloc] free [%llu-%llu]\n", freed.startBlock, freed.startBlock + freed.blockCount - 1);

        int32_t leftIdx = freeSpace.predecessorOfKey(freed.startBlock);
        bool    leftAdj = leftIdx != RBTREE_NIL &&
                           freeSpace.extentAt(leftIdx).startBlock + freeSpace.extentAt(leftIdx).blockCount == freed.startBlock;

        int32_t rightIdx = freeSpace.findExact(freed.startBlock + freed.blockCount);
        bool    rightAdj = rightIdx != RBTREE_NIL;

        if (leftAdj && rightAdj)
        {
            Extent merged{freeSpace.extentAt(leftIdx).startBlock,
                           freeSpace.extentAt(leftIdx).blockCount + freed.blockCount + freeSpace.extentAt(rightIdx).blockCount};
            freeSpace.remove(rightIdx);
            freeSpace.resize(leftIdx, merged);
        }
        else if (leftAdj)
        {
            freeSpace.resize(leftIdx, Extent{freeSpace.extentAt(leftIdx).startBlock,
                                              freeSpace.extentAt(leftIdx).blockCount + freed.blockCount});
        }
        else if (rightAdj)
        {
            freeSpace.resize(rightIdx, Extent{freed.startBlock, freed.blockCount + freeSpace.extentAt(rightIdx).blockCount});
        }
        else
        {
            insertFreeExtent(freed);
        }

        table.extentCount = freeSpace.count();
    }

    uint32_t freeExtentCount() const { return freeSpace.count(); }
    uint64_t freeBlockTotal() const { return freeSpace.totalFreeBlocks(); }

    const ExtentTableHeader &getHeader() const { return table; }
    const ExtentDescriptor &getMetadataDescriptor() const { return table.metadata; }
    uint64_t getMetadataSectorCount() const { return table.metadataSectors; }

    uint32_t enumerateFreeExtents(Extent *out, uint32_t maxOut) const { return freeSpace.enumerate(out, maxOut); }

    uint32_t enumerateDescriptor(const ExtentDescriptor &desc, Extent *out, uint32_t maxOut) const
    {
        uint32_t n = 0;

        for (uint32_t i = 0; i < desc.directCount && n < maxOut; i++)
            out[n++] = desc.direct[i];

        if (blockIo == nullptr)
            return n;

        if (desc.l1Lba != 0 && n < maxOut)
            n += collectLevel(desc.l1Lba, 1, out + n, maxOut - n);
        if (desc.l2Lba != 0 && n < maxOut)
            n += collectLevel(desc.l2Lba, 2, out + n, maxOut - n);
        if (desc.l3Lba != 0 && n < maxOut)
            n += collectLevel(desc.l3Lba, 3, out + n, maxOut - n);

        return n;
    }

    bool growDescriptor(ExtentDescriptor &desc, uint64_t additionalBlocks)
    {
        uint64_t obtained = 0;

        while (obtained < additionalBlocks)
        {
            uint64_t remaining = additionalBlocks - obtained;
            Extent chunk = allocateBestFit(remaining);
            if (!chunk.valid())
            {
                String::Printf("[extalloc] growDescriptor: insufficient free space (%llu/%llu blocks)\n",
                                obtained, additionalBlocks);
                return false;
            }

            if (!appendExtentToDescriptor(desc, chunk))
            {
                String::Printf("[extalloc] growDescriptor: extent descriptor exhausted (direct+L1+L2+L3 full)\n");
                free(chunk);
                return false;
            }

            obtained += chunk.blockCount;
        }

        return true;
    }

    bool growDescriptorInGroups(ExtentDescriptor &desc, uint64_t additionalBlocks,
                                 const BlockGroupDescriptor *groups, uint32_t groupCount,
                                 uint32_t preferredGroup)
    {
        uint64_t obtained = 0;

        while (obtained < additionalBlocks)
        {
            uint64_t remaining = additionalBlocks - obtained;
            Extent chunk{0, 0};
            bool gotChunk = false;

            for (uint32_t gi = 0; gi < groupCount && !gotChunk; gi++)
            {
                uint32_t g = (preferredGroup + gi) % groupCount;
                uint64_t rangeStart = groups[g].startBlock;
                uint64_t rangeEnd   = groups[g].startBlock + groups[g].blockCount - 1;

                RangeCandidate candidate;
                if (!findCandidateInRange(remaining, rangeStart, rangeEnd, /*allowPartial=*/true, candidate))
                    continue;

                uint64_t take = (candidate.rangeCount < remaining) ? candidate.rangeCount : remaining;
                chunk = takeFromChosen(candidate, take);
                gotChunk = chunk.valid();
            }

            if (!gotChunk)
            {
                String::Printf("[extalloc] growDescriptorInGroups: out of space (%llu/%llu blocks)\n",
                                obtained, additionalBlocks);
                return false;
            }

            if (!appendExtentToDescriptor(desc, chunk))
            {
                String::Printf("[extalloc] growDescriptorInGroups: descriptor exhausted\n");
                free(chunk);
                return false;
            }

            obtained += chunk.blockCount;
        }

        return true;
    }

    void shrinkDescriptor(ExtentDescriptor &desc, uint64_t newBlockCount)
    {
        uint64_t budget = newBlockCount;

        uint32_t keepDirect = 0;
        for (uint32_t i = 0; i < desc.directCount; i++)
        {
            Extent e = desc.direct[i];

            if (budget == 0)
            {
                free(e);
                continue;
            }

            if (e.blockCount <= budget)
            {
                budget -= e.blockCount;
                desc.direct[keepDirect++] = e;
            }
            else
            {
                Extent keep{e.startBlock, budget};
                Extent drop{e.startBlock + budget, e.blockCount - budget};
                free(drop);
                desc.direct[keepDirect++] = keep;
                budget = 0;
            }
        }
        desc.directCount = keepDirect;

        if (blockIo == nullptr)
            return;

        if (desc.l1Lba != 0 && shrinkLevel(desc.l1Lba, 1, budget))
        {
            free(Extent{desc.l1Lba, 1});
            desc.l1Lba = 0;
        }
        if (desc.l2Lba != 0 && shrinkLevel(desc.l2Lba, 2, budget))
        {
            free(Extent{desc.l2Lba, 1});
            desc.l2Lba = 0;
        }
        if (desc.l3Lba != 0 && shrinkLevel(desc.l3Lba, 3, budget))
        {
            free(Extent{desc.l3Lba, 1});
            desc.l3Lba = 0;
        }
    }

    void freeDescriptor(ExtentDescriptor &desc)
    {
        for (uint32_t i = 0; i < desc.directCount; i++)
            free(desc.direct[i]);

        if (blockIo != nullptr)
        {
            if (desc.l1Lba != 0) freeLevel(desc.l1Lba, 1);
            if (desc.l2Lba != 0) freeLevel(desc.l2Lba, 2);
            if (desc.l3Lba != 0) freeLevel(desc.l3Lba, 3);
        }

        desc.directCount = 0;
        desc.reserved0   = 0;
        desc.l1Lba       = 0;
        desc.l2Lba       = 0;
        desc.l3Lba       = 0;
    }

    uint32_t enumerateMetadataExtents(Extent *out, uint32_t maxOut) const
    {
        return enumerateDescriptor(table.metadata, out, maxOut);
    }

    void serializeEntries(uint8_t *buf, uint32_t bufBytes) const
    {
        for (uint32_t b = 0; b < bufBytes; b++)
            buf[b] = 0;

        uint32_t maxEntries = bufBytes / sizeof(Extent);
        freeSpace.enumerate((Extent *)buf, maxEntries);
    }

private:
    ExtentTableHeader table;
    FreeSpaceIndex    freeSpace;

    uint32_t          sectorSize;
    IMetadataBlockIo *blockIo = nullptr;

    struct RangeCandidate
    {
        Extent   node;
        uint64_t rangeStart;
        uint64_t rangeCount;
    };

    uint32_t capacityForSectors(uint32_t sectors) const
    {
        return (uint32_t)((sectors * (uint64_t)sectorSize) / sizeof(Extent));
    }

    uint32_t extentsPerBlock() const { return sectorSize / sizeof(Extent); }
    uint32_t lbasPerBlock() const { return sectorSize / sizeof(uint64_t); }

    void insertFreeExtent(const Extent &ext)
    {
        if (table.extentCount >= table.extentCapacity)
            growMetadata();

        if (freeSpace.count() >= TEMRIXFS_MAX_FREE_EXTENTS)
        {
            String::Printf("[extalloc] WARNING: free extent table full, dropping insert\n");
            return;
        }

        if (freeSpace.insert(ext) == RBTREE_NIL)
            String::Printf("[extalloc] WARNING: node pool exhausted, dropping insert\n");
    }

    void growMetadata()
    {
        uint64_t currentSectors = table.metadataSectors;
        uint64_t newSectors     = (currentSectors == 0) ? 1 : currentSectors * 2;
        uint64_t needed         = newSectors - currentSectors;

        if (!growDescriptor(table.metadata, needed))
        {
            String::Printf("[extalloc] ERROR: cannot grow metadata\n");
            return;
        }

        table.metadataSectors = currentSectors + needed;
        table.extentCapacity  = capacityForSectors((uint32_t)table.metadataSectors);

        String::Printf("[extalloc] grew metadata to %llu sectors across extent tree (capacity %u)\n",
                        table.metadataSectors, table.extentCapacity);
    }

    Extent allocateBestFit(uint64_t maxBlocks)
    {
        Extent failure{0, 0};
        if (maxBlocks == 0)
            return failure;

        int32_t idx = freeSpace.findBestFit(maxBlocks);
        uint64_t take;

        if (idx != RBTREE_NIL)
        {
            take = maxBlocks;
        }
        else
        {
            idx = freeSpace.findLargest();
            if (idx == RBTREE_NIL)
                return failure;

            take = freeSpace.extentAt(idx).blockCount;
            if (take > maxBlocks)
                take = maxBlocks;
        }

        Extent src = freeSpace.extentAt(idx);
        Extent result{src.startBlock, take};

        if (src.blockCount == take)
            freeSpace.remove(idx);
        else
            freeSpace.resize(idx, Extent{src.startBlock + take, src.blockCount - take});

        table.extentCount = freeSpace.count();
        return result;
    }

    bool findCandidateInRange(uint64_t requestedBlocks, uint64_t rangeStart, uint64_t rangeEnd,
                               bool allowPartial, RangeCandidate &outChosen) const
    {
        Extent all[TEMRIXFS_MAX_FREE_EXTENTS];
        uint32_t n = freeSpace.enumerate(all, TEMRIXFS_MAX_FREE_EXTENTS);

        bool           foundExact = false;
        RangeCandidate bestExact{Extent{0, 0}, 0, 0};
        RangeCandidate bestLargest{Extent{0, 0}, 0, 0};

        for (uint32_t i = 0; i < n; i++)
        {
            uint64_t s = all[i].startBlock;
            uint64_t e = s + all[i].blockCount - 1;

            uint64_t iStart = (s > rangeStart) ? s : rangeStart;
            uint64_t iEnd   = (e < rangeEnd) ? e : rangeEnd;
            if (iStart > iEnd)
                continue; 

            uint64_t avail = iEnd - iStart + 1;

            if (avail >= requestedBlocks && (!foundExact || avail < bestExact.rangeCount))
            {
                bestExact  = RangeCandidate{all[i], iStart, avail};
                foundExact = true;
            }

            if (avail > bestLargest.rangeCount)
                bestLargest = RangeCandidate{all[i], iStart, avail};
        }

        if (foundExact)
        {
            outChosen = bestExact;
            return true;
        }

        if (allowPartial && bestLargest.rangeCount > 0)
        {
            outChosen = bestLargest;
            return true;
        }

        return false;
    }

    Extent takeFromChosen(const RangeCandidate &chosen, uint64_t take)
    {
        int32_t idx = freeSpace.findExact(chosen.node.startBlock);
        if (idx == RBTREE_NIL)
            return Extent{0, 0};

        Extent node = freeSpace.extentAt(idx);
        freeSpace.remove(idx);

        uint64_t nodeStart = node.startBlock;
        uint64_t nodeEnd    = node.startBlock + node.blockCount - 1;
        uint64_t takeStart  = chosen.rangeStart;
        uint64_t takeEnd    = takeStart + take - 1;

        if (takeStart > nodeStart)
            insertFreeExtent(Extent{nodeStart, takeStart - nodeStart});

        if (takeEnd < nodeEnd)
            insertFreeExtent(Extent{takeEnd + 1, nodeEnd - takeEnd});

        table.extentCount = freeSpace.count();
        return Extent{takeStart, take};
    }

    
    
    bool appendExtentToDescriptor(ExtentDescriptor &desc, Extent chunk)
    {
        if (desc.directCount < TEMRIXFS_META_DIRECT_EXTENTS)
        {
            desc.direct[desc.directCount++] = chunk;
            return true;
        }

        if (blockIo == nullptr)
        {
            String::Printf("[extalloc] ERROR: direct extents exhausted and no block I/O bound for indirect levels\n");
            return false;
        }

        if (insertLevel(desc.l1Lba, 1, chunk))
            return true;

        if (insertLevel(desc.l2Lba, 2, chunk))
            return true;

        if (insertLevel(desc.l3Lba, 3, chunk))
            return true;

        return false;
    }

    void zeroBlock(uint8_t *buf) const
    {
        for (uint32_t i = 0; i < sectorSize && i < TEMRIXFS_MAX_SECTOR_SIZE; i++)
            buf[i] = 0;
    }

    bool insertLevel(uint64_t &lba, uint32_t level, Extent chunk)
    {
        uint8_t raw[TEMRIXFS_MAX_SECTOR_SIZE];

        if (lba == 0)
        {
            Extent blk = allocate(1);
            if (!blk.valid())
                return false;

            lba = blk.startBlock;
            zeroBlock(raw);
            if (!blockIo->writeBlock(lba, raw))
                return false;
        }

        if (level == 1)
        {
            if (!blockIo->readBlock(lba, raw))
                return false;

            Extent *arr = (Extent *)raw;
            uint32_t count = extentsPerBlock();

            for (uint32_t i = 0; i < count; i++)
            {
                if (arr[i].blockCount == 0)
                {
                    arr[i] = chunk;
                    return blockIo->writeBlock(lba, raw);
                }
            }

            return false; 
        }

        if (!blockIo->readBlock(lba, raw))
            return false;

        uint64_t *arr = (uint64_t *)raw;
        uint32_t count = lbasPerBlock();

        for (uint32_t i = 0; i < count; i++)
        {
            uint64_t before = arr[i];
            if (insertLevel(arr[i], level - 1, chunk))
            {
                if (arr[i] != before)
                    blockIo->writeBlock(lba, raw);
                return true;
            }
        }

        return false; 
    }

    uint32_t collectLevel(uint64_t lba, uint32_t level, Extent *out, uint32_t maxOut) const
    {
        if (lba == 0 || maxOut == 0 || blockIo == nullptr)
            return 0;

        uint8_t raw[TEMRIXFS_MAX_SECTOR_SIZE];
        if (!blockIo->readBlock(lba, raw))
            return 0;

        uint32_t n = 0;

        if (level == 1)
        {
            Extent *arr = (Extent *)raw;
            uint32_t count = extentsPerBlock();

            for (uint32_t i = 0; i < count && n < maxOut; i++)
            {
                if (arr[i].blockCount != 0)
                    out[n++] = arr[i];
            }

            return n;
        }

        uint64_t *arr = (uint64_t *)raw;
        uint32_t count = lbasPerBlock();

        for (uint32_t i = 0; i < count && n < maxOut; i++)
        {
            if (arr[i] != 0)
                n += collectLevel(arr[i], level - 1, out + n, maxOut - n);
        }

        return n;
    }

    bool shrinkLevel(uint64_t lba, uint32_t level, uint64_t &budget)
    {
        uint8_t raw[TEMRIXFS_MAX_SECTOR_SIZE];
        if (!blockIo->readBlock(lba, raw))
        {
            String::Printf("[extalloc] shrinkLevel: readBlock(%llu) failed\n", lba);
            return false;
        }

        bool dirty   = false;
        bool anyLive = false;

        if (level == 1)
        {
            Extent *arr = (Extent *)raw;
            uint32_t count = extentsPerBlock();

            for (uint32_t i = 0; i < count; i++)
            {
                if (arr[i].blockCount == 0)
                    continue;

                if (budget == 0)
                {
                    free(arr[i]);
                    arr[i] = Extent{0, 0};
                    dirty = true;
                    continue;
                }

                if (arr[i].blockCount <= budget)
                {
                    budget -= arr[i].blockCount;
                    anyLive = true;
                }
                else
                {
                    Extent keep{arr[i].startBlock, budget};
                    Extent drop{arr[i].startBlock + budget, arr[i].blockCount - budget};
                    free(drop);
                    arr[i] = keep;
                    budget = 0;
                    dirty  = true;
                    anyLive = true;
                }
            }
        }
        else
        {
            uint64_t *arr = (uint64_t *)raw;
            uint32_t count = lbasPerBlock();

            for (uint32_t i = 0; i < count; i++)
            {
                if (arr[i] == 0)
                    continue;

                if (shrinkLevel(arr[i], level - 1, budget))
                {
                    free(Extent{arr[i], 1});
                    arr[i] = 0;
                    dirty  = true;
                }
                else
                {
                    anyLive = true;
                }
            }
        }

        if (dirty)
            blockIo->writeBlock(lba, raw);

        return !anyLive;
    }

    void freeLevel(uint64_t lba, uint32_t level)
    {
        if (lba == 0 || blockIo == nullptr)
            return;

        uint8_t raw[TEMRIXFS_MAX_SECTOR_SIZE];
        if (!blockIo->readBlock(lba, raw))
            return;

        if (level == 1)
        {
            Extent *arr = (Extent *)raw;
            uint32_t count = extentsPerBlock();

            for (uint32_t i = 0; i < count; i++)
            {
                if (arr[i].blockCount != 0)
                    free(arr[i]);
            }
        }
        else
        {
            uint64_t *arr = (uint64_t *)raw;
            uint32_t count = lbasPerBlock();

            for (uint32_t i = 0; i < count; i++)
            {
                if (arr[i] != 0)
                    freeLevel(arr[i], level - 1);
            }
        }

        free(Extent{lba, 1});
    }
};