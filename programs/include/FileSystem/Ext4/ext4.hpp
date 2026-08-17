#pragma once

#include <temrixstd.h>
#include "nvme.hpp"
#include "FileSystem/gpt.hpp"
#include "FileSystem/FsShared.hpp"

#define GPT_LINUX_DATA_GUID         { 0xAF,0x3D,0xC6,0x0F,0x83,0x84,0x72,0x47, \
                                      0x8E,0x79,0x3D,0x69,0xD8,0x47,0x7D,0xE4 }

#define EXT4_SUPERBLOCK_MAGIC       0xEF53
#define EXT4_EXTENT_HEADER_MAGIC    0xF30A
#define EXT4_SUPERBLOCK_OFFSET_LBA  2
#define EXT4_SUPERBLOCK_SECTORS     2
#define EXT4_SUPERBLOCK_MAGIC_OFF   56
#define EXT4_ROOT_INODE             2

#define EXT4_SECTOR_SIZE            512
#define EXT4_BLOCK_SIZE_BASE        1024

#define EXT4_ASYNC_BATCH_SIZE       ((NVME_QUEUE_DEPTH / 2) - 1)
#define EXT4_ASYNC_NUM_POOLS        2
#define EXT4_MAX_PRP_PER_POOL       32          

struct __attribute__((packed)) Ext4Superblock {
    uint32_t inodesCount, blocksCountLo, rBlocksCountLo, freeBlocksCountLo;
    uint32_t freeInodesCount, firstDataBlock, logBlockSize, logClusterSize;
    uint32_t blocksPerGroup, clustersPerGroup, inodesPerGroup;
    uint32_t mtime, wtime;
    uint16_t mntCount, maxMntCount, magic, state, errors, minorRevLevel;
    uint32_t lastcheck, checkinterval, creatorOs, revLevel;
    uint16_t defResuid, defResgid;
    uint32_t firstIno;
    uint16_t inodeSize, blockGroupNr;
    uint32_t featureCompat, featureIncompat, featureRoCompat;
    uint8_t  uuid[16], volumeName[16], lastMounted[64];
};

struct __attribute__((packed)) Ext4Bgd {
    uint32_t blockBitmapLo, inodeBitmapLo, inodeTableLo;
    uint16_t freeBlocksCountLo, freeInodesCountLo, usedDirsCountLo, flags;
    uint32_t excludeBitmapLo;
    uint16_t blockBitmapChecksumLo, inodeBitmapChecksumLo, itableUnusedLo, checksum;
    uint32_t blockBitmapHi, inodeBitmapHi, inodeTableHi;
    uint16_t freeBlocksCountHi, freeInodesCountHi, usedDirsCountHi, itableUnusedHi;
    uint32_t excludeBitmapHi;
    uint16_t blockBitmapChecksumHi, inodeBitmapChecksumHi;
    uint32_t reserved;
};

struct __attribute__((packed)) Ext4Inode {
    uint16_t mode, uid;
    uint32_t sizeLo, atime, ctime, mtime, dtime;
    uint16_t gid, linksCount;
    uint32_t blocksLo, flags, osd1;
    uint8_t  block[60];
    uint32_t generation, fileAclLo, sizeHi, obsoFaddr;
    uint8_t  osd2[12];
    uint16_t extraIsize, checksumHi;
    uint32_t ctimeExtra, mtimeExtra, atimeExtra, crtime, crtimeExtra, versionHi, projid;
};

struct __attribute__((packed)) Ext4ExtentHeader {
    uint16_t magic, entries, max, depth;
    uint32_t generation;
};

struct __attribute__((packed)) Ext4ExtentIndex {
    uint32_t block, leafLo;
    uint16_t leafHi, unused;
};

struct __attribute__((packed)) Ext4Extent {
    uint32_t block;
    uint16_t len, startHi;
    uint32_t startLo;
};

struct __attribute__((packed)) Ext4DirEntry {
    uint32_t inode;
    uint16_t recLen;
    uint8_t  nameLen, fileType;
    char     name[];
};

static inline char* u64ToStr(uint64_t val, char* buf) {
    buf[20] = '\0';
    int i = 19;
    if (val == 0) { buf[i--] = '0'; }
    else { while (val) { buf[i--] = '0' + (val % 10); val /= 10; } }
    return &buf[i + 1];
}

static void printHex8(const char* label, uint8_t* data, int offset, int len) {
    String::Print(label);
    for (int i = 0; i < len; i++) {
        uint8_t b = data[offset + i];
        char hex[3];
        hex[0] = "0123456789ABCDEF"[b >> 4];
        hex[1] = "0123456789ABCDEF"[b & 0xF];
        hex[2] = ' ';
        Syscall::IO::Write(hex, 3);
    }
    String::Print("\n");
}

class Ext4 {
public:
    bool init(NvmeController* nvme) {
        this->nvme       = nvme;
        this->dmaBufVirt = 0;
        this->dmaBufPhys = 0;
        this->dmaBufSize = 0;

        Syscall::Memory::DmaAllocResult prpDma;
        if (Syscall::Memory::AllocDma(NVME_QUEUE_DEPTH * NVME_PAGE_SIZE,
                ::Memory::Read | ::Memory::Write | ::Memory::User | ::Memory::NoCache,
                &prpDma) != 0)
            return false;

        for (int i = 0; i < NVME_QUEUE_DEPTH; i++) {
            prpListVirt[i] = prpDma.virt + (uint64_t)i * NVME_PAGE_SIZE;
            prpListPhys[i] = prpDma.phys + (uint64_t)i * NVME_PAGE_SIZE;
            for (int b = 0; b < (int)NVME_PAGE_SIZE; b++)
                ((uint8_t*)prpListVirt[i])[b] = 0;
        }

        static const uint8_t linuxGuid[GPT_GUID_LEN] = GPT_LINUX_DATA_GUID;

        uint8_t* gpt = (uint8_t*)Syscall::Memory::Map(EXT4_SECTOR_SIZE);
        readSectors(GPT_HEADER_LBA, 1, gpt);

        printHex8("[ext4] gpt[0..7]:   ", gpt,  0, GPT_SIG_LEN);
        printHex8("[ext4] gpt[72..79]: ", gpt, GPT_OFF_ENTRY_LBA, 8);
        printHex8("[ext4] gpt[80..83]: ", gpt, GPT_OFF_NUM_PARTS, 4);
        printHex8("[ext4] gpt[84..87]: ", gpt, GPT_OFF_ENTRY_SIZE, 4);

        static const char gptSig[GPT_SIG_LEN] = { 'E','F','I',' ','P','A','R','T' };
        for (int i = 0; i < GPT_SIG_LEN; i++) {
            if (gpt[i] != (uint8_t)gptSig[i]) { String::Print("[ext4] no GPT\n"); return false; }
        }

        uint64_t entryLba  = *((uint64_t*)(gpt + GPT_OFF_ENTRY_LBA));
        uint32_t numParts  = *((uint32_t*)(gpt + GPT_OFF_NUM_PARTS));
        uint32_t entrySize = *((uint32_t*)(gpt + GPT_OFF_ENTRY_SIZE));

        char tbuf[21];
        String::Print("[ext4] entry_lba=");  String::Print(u64ToStr(entryLba,  tbuf)); String::Print("\n");
        String::Print("[ext4] num_parts=");  String::Print(u64ToStr(numParts,  tbuf)); String::Print("\n");
        String::Print("[ext4] entry_size="); String::Print(u64ToStr(entrySize, tbuf)); String::Print("\n");

        uint32_t entrySectors = (numParts * entrySize + (EXT4_SECTOR_SIZE - 1)) / EXT4_SECTOR_SIZE;
        String::Print("[ext4] entrySectors="); String::Print(u64ToStr(entrySectors, tbuf)); String::Print("\n");

        uint8_t* entries = (uint8_t*)Syscall::Memory::Map(numParts * entrySize);
        readSectors(entryLba, entrySectors, entries);

        printHex8("[ext4] part0[0..15] (type GUID):  ", entries,  0, GPT_GUID_LEN);
        printHex8("[ext4] part0[16..31] (uniq GUID): ", entries, 16, GPT_GUID_LEN);
        printHex8("[ext4] part0[32..39] (start LBA): ", entries, GPT_PART_OFF_START_LBA, 8);

        partitionStart = 0;
        for (uint32_t i = 0; i < numParts && !partitionStart; i++) {
            uint8_t* part = entries + i * entrySize;

            bool empty = true;
            for (int j = 0; j < GPT_GUID_LEN; j++) if (part[j]) { empty = false; break; }
            if (empty) {
                String::Print("[ext4] part ");
                String::Print(u64ToStr(i, tbuf));
                String::Print(" empty, skipping\n");
                continue;
            }

            String::Print("[ext4] checking part ");
            String::Print(u64ToStr(i, tbuf));
            printHex8(" type: ", part, 0, GPT_GUID_LEN);

            bool match = true;
            for (int j = 0; j < GPT_GUID_LEN; j++)
                if (part[j] != linuxGuid[j]) { match = false; break; }
            if (!match) { String::Print("[ext4] GUID mismatch\n"); continue; }

            uint64_t startLba = *((uint64_t*)(part + GPT_PART_OFF_START_LBA));
            String::Print("[ext4] GUID match! start_lba=");
            String::Print(u64ToStr(startLba, tbuf));
            String::Print("\n");

            uint8_t sbBuf[EXT4_SECTOR_SIZE * EXT4_SUPERBLOCK_SECTORS];
            readSectors(startLba + EXT4_SUPERBLOCK_OFFSET_LBA,
                            EXT4_SUPERBLOCK_SECTORS, sbBuf);

            printHex8("[ext4] sb_buf[0..15]:          ", sbBuf,  0, 16);
            printHex8("[ext4] sb_buf[56..57] (magic): ", sbBuf, EXT4_SUPERBLOCK_MAGIC_OFF, 2);

            uint16_t magic = *((uint16_t*)(sbBuf + EXT4_SUPERBLOCK_MAGIC_OFF));
            String::Print("[ext4] magic=0x");
            {
                char hex[5];
                hex[0] = "0123456789ABCDEF"[(magic >> 12) & 0xF];
                hex[1] = "0123456789ABCDEF"[(magic >>  8) & 0xF];
                hex[2] = "0123456789ABCDEF"[(magic >>  4) & 0xF];
                hex[3] = "0123456789ABCDEF"[(magic >>  0) & 0xF];
                hex[4] = '\0';
                String::Print(hex);
            }
            String::Print("\n");

            if (magic != EXT4_SUPERBLOCK_MAGIC) {
                String::Print("[ext4] bad magic, skipping\n");
                continue;
            }

            String::Print("[ext4] found partition\n");
            partitionStart = startLba;
        }

        if (!partitionStart) { String::Print("[ext4] partition not found\n"); return false; }

        uint8_t* sbBuf = (uint8_t*)Syscall::Memory::Map(EXT4_BLOCK_SIZE_BASE);
        readSectors(partitionStart + EXT4_SUPERBLOCK_OFFSET_LBA,
                        EXT4_SUPERBLOCK_SECTORS, sbBuf);
        Ext4Superblock* sb = (Ext4Superblock*)sbBuf;
        if (sb->magic != EXT4_SUPERBLOCK_MAGIC) { String::Print("[ext4] bad superblock\n"); return false; }

        blockSize      = EXT4_BLOCK_SIZE_BASE << sb->logBlockSize;
        inodesPerGroup = sb->inodesPerGroup;
        inodeSize      = sb->inodeSize;
        blocksPerGroup = sb->blocksPerGroup;

        String::Print("[ext4] init OK\n");
        char tbuf2[21];
        String::Print("[fs] blockSize=");
        String::Print(u64ToStr(blockSize, tbuf2));
        String::Print("\n");
        return true;
    }

    uint32_t fileSize(const char *path)
    {
        uint32_t inodeNum = resolvePath(path);
        if (!inodeNum) return 0;

        Ext4Inode inode;
        readInode(inodeNum, &inode);

        return inode.sizeLo | ((uint64_t)inode.sizeHi << 32);
    }

    bool statOrReadFile(const char* path, uint8_t* buf, uint32_t* sizeOut, bool* foundOut)
    {
        uint32_t inodeNum = resolvePath(path);
        if (!inodeNum) {
            *sizeOut = 0;
            *foundOut = false;
            return false;
        }
        *foundOut = true;

        Ext4Inode inode;
        readInode(inodeNum, &inode);

        uint64_t fileSize = inode.sizeLo | ((uint64_t)inode.sizeHi << 32);
        *sizeOut = (uint32_t)fileSize;

        if (!buf) return true;  

        uint32_t numBlocks = (uint32_t)((fileSize + blockSize - 1) / blockSize);
        if (numBlocks == 0) return true;

        uint64_t* physBlocks = (uint64_t*)Syscall::Memory::Map(numBlocks * sizeof(uint64_t));
        uint32_t collected = collectBlocks(&inode, physBlocks, numBlocks);
        if (collected < numBlocks) {
            String::Print("[ext4] extent collection incomplete\n");
            return false;
        }

        static const int B = EXT4_ASYNC_BATCH_SIZE;

        uint32_t poolBytes = (uint32_t)B * blockSize;
        Syscall::Memory::DmaAllocResult pool0, pool1;
        if (Syscall::Memory::AllocDma(poolBytes, ::Memory::Read | ::Memory::Write | ::Memory::User | ::Memory::NoCache, &pool0) != 0) return false;
        if (Syscall::Memory::AllocDma(poolBytes, ::Memory::Read | ::Memory::Write | ::Memory::User | ::Memory::NoCache, &pool1) != 0) return false;
        uint64_t poolVirt[2] = { pool0.virt, pool1.virt };
        uint64_t poolPhys[2] = { pool0.phys, pool1.phys };

        uint64_t prpPhys[EXT4_ASYNC_NUM_POOLS][EXT4_MAX_PRP_PER_POOL];
        for (int p = 0; p < EXT4_ASYNC_NUM_POOLS; p++) {
            for (int i = 0; i < B; i++) {
                int      slot    = p * B + i;
                uint64_t bufPhys = poolPhys[p] + (uint64_t)i * blockSize;
                PrpList list;
                list.virt  = (uint64_t*)prpListVirt[slot];
                list.phys  = prpListPhys[slot];
                list.count = 0;
                list.build(bufPhys, blockSize);
                prpPhys[p][i] = list.phys;
            }
        }

        uint32_t sectorsPerBlock = blockSize / EXT4_SECTOR_SIZE;

        auto submitBatch = [&](int p, uint32_t start, int batchN) {
            for (int i = 0; i < batchN; i++) {
                uint64_t lba     = partitionStart + physBlocks[start + i] * sectorsPerBlock;
                uint64_t bufPhys = poolPhys[p] + (uint64_t)i * blockSize;
                nvme->readAsync(lba, sectorsPerBlock, bufPhys,
                                    EXT4_SECTOR_SIZE, prpPhys[p][i]);
            }
        };

        auto drainAndCopy = [&](int p, int batchN, uint64_t byteOffset) {
            nvme->drainAll();
            uint8_t* src = (uint8_t*)poolVirt[p];
            for (int i = 0; i < batchN; i++) {
                uint64_t off    = byteOffset + (uint64_t)i * blockSize;
                uint32_t toCopy = blockSize;
                if (off + toCopy > fileSize) toCopy = (uint32_t)(fileSize - off);
                Memory::Copy(buf + off, src + (uint64_t)i * blockSize, toCopy);
            }
        };

        uint32_t blocksDone = 0;

        int curN = (int)(numBlocks - blocksDone); if (curN > B) curN = B;
        submitBatch(0, 0, curN);
        blocksDone += curN;

        int      prevPool = 0;
        int      prevN    = curN;
        uint64_t prevOff  = 0;

        while (blocksDone < numBlocks) {
            int      nextPool = prevPool ^ 1;
            int      nextN    = (int)(numBlocks - blocksDone); if (nextN > B) nextN = B;
            uint64_t nextOff  = prevOff + (uint64_t)prevN * blockSize;

            submitBatch(nextPool, blocksDone, nextN);
            blocksDone += nextN;

            drainAndCopy(prevPool, prevN, prevOff);

            prevPool = nextPool;
            prevN    = nextN;
            prevOff  = nextOff;
        }

        drainAndCopy(prevPool, prevN, prevOff);
        return true;
    }

    bool readFile(const char* path, uint8_t* buf, uint32_t* sizeOut) {
        bool found;
        return statOrReadFile(path, buf, sizeOut, &found) && found;
    }

    uint32_t listDirectory(const char* path, FsDirEntry* out, uint32_t maxEntries)
    {
        uint32_t dirInode = resolvePath(path);
        if (!dirInode) return 0;

        Ext4Inode inode;
        readInode(dirInode, &inode);

        uint64_t size = inode.sizeLo | ((uint64_t)inode.sizeHi << 32);
        uint32_t numBlocks = (uint32_t)((size + blockSize - 1) / blockSize);

        uint8_t* buf = (uint8_t*)Syscall::Memory::Map(blockSize);
        uint32_t count = 0;

        for (uint32_t b = 0; b < numBlocks; b++) {
            uint64_t phys = getPhysicalBlock(&inode, b);
            if (!phys) break;
            readBlock(phys, buf);

            uint32_t off = 0;
            while (off < blockSize) {
                Ext4DirEntry* de = (Ext4DirEntry*)(buf + off);
                if (!de->recLen) break;

                bool isDot   = de->nameLen == 1 && de->name[0] == '.';
                bool isDotDot = de->nameLen == 2 && de->name[0] == '.' && de->name[1] == '.';

                if (de->inode && de->nameLen > 0 && !isDot && !isDotDot) {
                    if (count < maxEntries) {
                        FsDirEntry &entry = out[count];
                        uint32_t n = de->nameLen < (FS_MAX_NAME - 1) ? de->nameLen : (FS_MAX_NAME - 1);
                        for (uint32_t k = 0; k < n; k++) entry.name[k] = de->name[k];
                        entry.name[n] = '\0';
                        entry.fileType = de->fileType;

                        Ext4Inode childInode;
                        readInode(de->inode, &childInode);
                        entry.size = childInode.sizeLo | ((uint64_t)childInode.sizeHi << 32);
                    }
                    count++;
                }

                off += de->recLen;
            }
        }
        return count;
    }

private:
    NvmeController* nvme;
    uint64_t        partitionStart;
    uint32_t        blockSize;
    uint32_t        inodesPerGroup;
    uint32_t        inodeSize;
    uint32_t        blocksPerGroup;
    uint64_t        dmaBufVirt;
    uint64_t        dmaBufPhys;
    uint32_t        dmaBufSize;

    uint64_t prpListVirt[NVME_QUEUE_DEPTH];
    uint64_t prpListPhys[NVME_QUEUE_DEPTH];

    void ensureDma(uint32_t bytes) {
        if (bytes > dmaBufSize) {
            Syscall::Memory::DmaAllocResult dma;
            if (Syscall::Memory::AllocDma(bytes, ::Memory::Read | ::Memory::Write | ::Memory::User | ::Memory::NoCache, &dma) != 0) return;
            dmaBufVirt = dma.virt;
            dmaBufPhys = dma.phys;
            dmaBufSize = bytes;
        }
    }

    void readSectors(uint64_t lba, uint32_t count, uint8_t* dst) {
        uint32_t bytes = count * EXT4_SECTOR_SIZE;
        ensureDma(bytes);

        PrpList list;
        list.virt  = (uint64_t*)prpListVirt[0];
        list.phys  = prpListPhys[0];
        list.count = 0;
        list.build(dmaBufPhys, bytes);

        nvme->readSectors(lba, count, dmaBufPhys, EXT4_SECTOR_SIZE, list.phys);
        Memory::Copy(dst, (uint8_t*)dmaBufVirt, bytes);
    }

    void readBlock(uint64_t block, uint8_t* buf) {
        uint32_t sectorsPerBlock = blockSize / EXT4_SECTOR_SIZE;
        uint64_t lba = partitionStart + block * sectorsPerBlock;

        ensureDma(blockSize);

        PrpList list;
        list.virt  = (uint64_t*)prpListVirt[0];
        list.phys  = prpListPhys[0];
        list.count = 0;
        list.build(dmaBufPhys, blockSize);

        nvme->readSectors(lba, sectorsPerBlock, dmaBufPhys, EXT4_SECTOR_SIZE, list.phys);
        Memory::Copy(buf, (uint8_t*)dmaBufVirt, blockSize);
    }

    void readBlocksBatch(uint64_t*    blocks,
                                    uint8_t**    dsts,
                                    int          n) {
        if (n <= 0) return;
        uint32_t sectorsPerBlock = blockSize / EXT4_SECTOR_SIZE;

        ensureDma((uint32_t)n * blockSize);

        uint64_t lbas[NVME_QUEUE_DEPTH];
        uint64_t bufs[NVME_QUEUE_DEPTH];
        uint64_t batchPrpPhys[NVME_QUEUE_DEPTH];

        for (int i = 0; i < n; i++) {
            lbas[i] = partitionStart + blocks[i] * sectorsPerBlock;
            bufs[i] = dmaBufPhys + (uint64_t)i * blockSize;

            PrpList list;
            list.virt        = (uint64_t*)prpListVirt[i];
            list.phys  = prpListPhys[i];
            list.count = 0;
            list.build(bufs[i], blockSize);
            batchPrpPhys[i]  = list.phys;
        }

        nvme->readSectorsBatch(lbas, bufs, batchPrpPhys, sectorsPerBlock, EXT4_SECTOR_SIZE, n);

        for (int i = 0; i < n; i++)
            Memory::Copy(dsts[i],
                    (uint8_t*)dmaBufVirt + (uint64_t)i * blockSize,
                    blockSize);
    }

    void readInode(uint32_t inodeNum, Ext4Inode* out) {
        uint32_t group = (inodeNum - 1) / inodesPerGroup;
        uint32_t index = (inodeNum - 1) % inodesPerGroup;

        uint32_t bgdtStart      = 1 + (blockSize == EXT4_BLOCK_SIZE_BASE ? 1 : 0);
        uint32_t descsPerBlock  = blockSize / sizeof(Ext4Bgd);
        uint32_t bgdtBlock      = bgdtStart + group / descsPerBlock;
        uint32_t bgdtIndex      = group % descsPerBlock;

        uint8_t* buf = (uint8_t*)Syscall::Memory::Map(blockSize);
        readBlock(bgdtBlock, buf);

        Ext4Bgd* bgd = (Ext4Bgd*)(buf + bgdtIndex * sizeof(Ext4Bgd));
        uint64_t inodeTableBlock = bgd->inodeTableLo;

        uint32_t inodesPerBlock = blockSize / inodeSize;
        uint32_t blockOffset    = index / inodesPerBlock;
        uint32_t inodeOffset    = (index % inodesPerBlock) * inodeSize;

        uint8_t* ibuf = (uint8_t*)Syscall::Memory::Map(blockSize);
        readBlock(inodeTableBlock + blockOffset, ibuf);
        Memory::Copy((uint8_t*)out, ibuf + inodeOffset, sizeof(Ext4Inode));
    }

    uint64_t getPhysicalBlock(Ext4Inode* inode, uint32_t logicalBlock) {
        Ext4ExtentHeader* hdr = (Ext4ExtentHeader*)inode->block;
        if (hdr->magic != EXT4_EXTENT_HEADER_MAGIC) {
            String::Print("[ext4] bad extent magic\n");
            return 0;
        }

        while (hdr->depth > 0) {
            Ext4ExtentIndex* idx    = (Ext4ExtentIndex*)(hdr + 1);
            Ext4ExtentIndex* chosen = idx;
            for (int i = 1; i < hdr->entries; i++) {
                if (idx[i].block <= logicalBlock) chosen = &idx[i];
                else break;
            }
            uint64_t childBlock = chosen->leafLo | ((uint64_t)chosen->leafHi << 32);
            uint8_t* buf = (uint8_t*)Syscall::Memory::Map(blockSize);
            readBlock(childBlock, buf);
            hdr = (Ext4ExtentHeader*)buf;
        }

        Ext4Extent* ext = (Ext4Extent*)(hdr + 1);
        for (int i = 0; i < hdr->entries; i++) {
            if (logicalBlock >= ext[i].block && logicalBlock < ext[i].block + ext[i].len) {
                uint64_t phys = ext[i].startLo | ((uint64_t)ext[i].startHi << 32);
                return phys + (logicalBlock - ext[i].block);
            }
        }
        String::Print("[ext4] block not found in extents\n");
        return 0;
    }

    uint32_t collectBlocks(Ext4Inode* inode,
                                    uint64_t* physBlocks, uint32_t maxBlocks) {
        Ext4ExtentHeader* hdr = (Ext4ExtentHeader*)inode->block;
        if (hdr->magic != EXT4_EXTENT_HEADER_MAGIC) return 0;

        if (hdr->depth == 0) {
            Ext4Extent* exts  = (Ext4Extent*)(hdr + 1);
            uint32_t total = 0;
            for (int i = 0; i < hdr->entries; i++) {
                uint64_t base = exts[i].startLo | ((uint64_t)exts[i].startHi << 32);
                for (uint32_t b = 0; b < exts[i].len; b++) {
                    if (total < maxBlocks) physBlocks[total] = base + b;
                    total++;
                }
            }
            return total;
        }

        uint64_t fileSize  = inode->sizeLo | ((uint64_t)inode->sizeHi << 32);
        uint32_t blockCount = (uint32_t)((fileSize + blockSize - 1) / blockSize);
        for (uint32_t i = 0; i < blockCount && i < maxBlocks; i++)
            physBlocks[i] = getPhysicalBlock(inode, i);
        return blockCount;
    }

    uint32_t lookupDir(uint32_t dirInode, const char* name) {
        Ext4Inode inode;
        readInode(dirInode, &inode);

        uint64_t size    = inode.sizeLo | ((uint64_t)inode.sizeHi << 32);
        uint32_t nameLen = String::Length(name);
        uint32_t numBlocks = (uint32_t)((size + blockSize - 1) / blockSize);

        uint32_t batchSize = numBlocks < (uint32_t)(NVME_QUEUE_DEPTH - 1)
                            ? numBlocks : (uint32_t)(NVME_QUEUE_DEPTH - 1);

        uint64_t physBlocks[NVME_QUEUE_DEPTH];
        uint8_t* blockBufs[NVME_QUEUE_DEPTH];
        uint8_t* memPool = (uint8_t*)Syscall::Memory::Map(blockSize * batchSize);

        for (uint32_t i = 0; i < batchSize; i++) {
            physBlocks[i] = getPhysicalBlock(&inode, i);
            blockBufs[i]  = memPool + (uint64_t)i * blockSize;
        }

        readBlocksBatch(physBlocks, blockBufs, (int)batchSize);

        for (uint32_t b = 0; b < batchSize; b++) {
            uint32_t off = 0;
            while (off < blockSize) {
                Ext4DirEntry* de = (Ext4DirEntry*)(blockBufs[b] + off);
                if (!de->recLen) break;
                if (de->inode && de->nameLen == nameLen &&
                    String::Compare(de->name, name, nameLen) == 0)
                    return de->inode;
                off += de->recLen;
            }
        }

        for (uint32_t b = batchSize; b < numBlocks; b++) {
            uint64_t phys = getPhysicalBlock(&inode, b);
            if (!phys) break;
            uint8_t* buf = (uint8_t*)Syscall::Memory::Map(blockSize);
            readBlock(phys, buf);
            uint32_t off = 0;
            while (off < blockSize) {
                Ext4DirEntry* de = (Ext4DirEntry*)(buf + off);
                if (!de->recLen) break;
                if (de->inode && de->nameLen == nameLen &&
                    String::Compare(de->name, name, nameLen) == 0)
                    return de->inode;
                off += de->recLen;
            }
        }

        return 0;
    }

    uint32_t resolvePath(const char* path) {
        uint32_t inode = EXT4_ROOT_INODE;
        int i = 0;
        if (path[i] == '/') i++;

        while (path[i]) {
            char component[256];
            int j = 0;
            while (path[i] && path[i] != '/') component[j++] = path[i++];
            component[j] = '\0';
            if (path[i] == '/') i++;
            if (j == 0) continue;

            inode = lookupDir(inode, component);
            if (!inode) { String::Print("[ext4] path component not found\n"); return 0; }
        }
        return inode;
    }
};