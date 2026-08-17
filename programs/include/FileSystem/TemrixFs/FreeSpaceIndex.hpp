#pragma once

#include <temrixstd.h>
#include "RbTree.hpp"
#include "Extent.hpp"

class FreeSpaceIndex
{
public:
    FreeSpaceIndex() : offsetTree(offsetPolicy), sizeTree(sizePolicy) { reset(); }

    void reset()
    {
        for (int32_t i = 0; i < (int32_t)TEMRIXFS_MAX_FREE_EXTENTS - 1; i++)
            pool[i].offLinks.left = i + 1;
        pool[TEMRIXFS_MAX_FREE_EXTENTS - 1].offLinks.left = RBTREE_NIL;

        freeSlotHead = 0;
        liveCount    = 0;
        totalBlocks  = 0;
        offsetTree.reset();
        sizeTree.reset();
    }

    uint32_t count() const { return liveCount; }
    uint64_t totalFreeBlocks() const { return totalBlocks; }
    const Extent &extentAt(int32_t idx) const { return pool[idx].ext; }

    
    int32_t insert(const Extent &ext)
    {
        int32_t idx = allocSlot();
        if (idx == RBTREE_NIL)
            return RBTREE_NIL;

        pool[idx].ext = ext;
        offsetTree.insert(idx);
        sizeTree.insert(idx);
        liveCount++;
        totalBlocks += ext.blockCount;
        return idx;
    }

    void remove(int32_t idx)
    {
        totalBlocks -= pool[idx].ext.blockCount;
        offsetTree.remove(idx);
        sizeTree.remove(idx);
        freeSlot(idx);
        liveCount--;
    }

    
    void resize(int32_t idx, const Extent &newExt)
    {
        totalBlocks += newExt.blockCount - pool[idx].ext.blockCount;
        offsetTree.remove(idx);
        sizeTree.remove(idx);
        pool[idx].ext = newExt;
        offsetTree.insert(idx);
        sizeTree.insert(idx);
    }

    int32_t findBestFit(uint64_t requested) const
    {
        return sizeTree.lowerBound([&](int32_t idx) { return pool[idx].ext.blockCount < requested; });
    }

    int32_t findLargest() const { return sizeTree.treeMaximum(); }

    int32_t findExact(uint64_t startBlock) const
    {
        int32_t idx = offsetTree.lowerBound([&](int32_t i) { return pool[i].ext.startBlock < startBlock; });
        return (idx != RBTREE_NIL && pool[idx].ext.startBlock == startBlock) ? idx : RBTREE_NIL;
    }

    int32_t predecessorOfKey(uint64_t key) const
    {
        return offsetTree.lastWhere([&](int32_t idx) { return pool[idx].ext.startBlock < key; });
    }

    uint32_t enumerate(Extent *out, uint32_t maxOut) const
    {
        uint32_t written = 0;
        auto visit = [&](int32_t idx) {
            if (written < maxOut)
                out[written++] = pool[idx].ext;
        };
        offsetTree.inOrder(visit);
        return written;
    }

private:
    struct FreeNode
    {
        Extent  ext;
        RbLinks offLinks;  
                           
        RbLinks sizeLinks;
    };

    class OffsetPolicy
    {
    public:
        explicit OffsetPolicy(FreeNode *p) : pool(p) {}
        RbLinks &links(int32_t idx) { return pool[idx].offLinks; }
        const RbLinks &links(int32_t idx) const { return pool[idx].offLinks; }
        bool less(int32_t a, int32_t b) const { return pool[a].ext.startBlock < pool[b].ext.startBlock; }
    private:
        FreeNode *pool;
    };

    class SizePolicy
    {
    public:
        explicit SizePolicy(FreeNode *p) : pool(p) {}
        RbLinks &links(int32_t idx) { return pool[idx].sizeLinks; }
        const RbLinks &links(int32_t idx) const { return pool[idx].sizeLinks; }
        bool less(int32_t a, int32_t b) const
        {
            const Extent &ea = pool[a].ext;
            const Extent &eb = pool[b].ext;
            if (ea.blockCount != eb.blockCount) return ea.blockCount < eb.blockCount;
            return ea.startBlock < eb.startBlock; 
        }
    private:
        FreeNode *pool;
    };

    int32_t allocSlot()
    {
        if (freeSlotHead == RBTREE_NIL) return RBTREE_NIL;
        int32_t idx  = freeSlotHead;
        freeSlotHead = pool[idx].offLinks.left;
        return idx;
    }

    void freeSlot(int32_t idx)
    {
        pool[idx].offLinks.left = freeSlotHead;
        freeSlotHead = idx;
    }

    FreeNode pool[TEMRIXFS_MAX_FREE_EXTENTS];
    int32_t  freeSlotHead = RBTREE_NIL;
    uint32_t liveCount    = 0;
    uint64_t totalBlocks  = 0;

    OffsetPolicy offsetPolicy{pool};
    SizePolicy   sizePolicy{pool};
    IntrusiveRbTree<OffsetPolicy> offsetTree;
    IntrusiveRbTree<SizePolicy>   sizeTree;
};