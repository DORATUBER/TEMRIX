#pragma once

#include <temrixstd.h>

struct Extent
{
    uint64_t startBlock;
    uint64_t blockCount;

    bool valid() const { return blockCount != 0; }
};

#define TEMRIXFS_EXTENT_MAGIC 0x54455854524D5846ULL

#define TEMRIXFS_META_DIRECT_EXTENTS 4
#define TEMRIXFS_MAX_SECTOR_SIZE 4096

struct ExtentDescriptor
{
    Extent direct[TEMRIXFS_META_DIRECT_EXTENTS];
    uint32_t directCount;
    uint32_t reserved0;
    uint64_t l1Lba;
    uint64_t l2Lba;
    uint64_t l3Lba;
};

struct ExtentTableHeader
{
    uint64_t magic;
    uint32_t version;
    uint32_t extentCount;
    uint32_t extentCapacity;
    uint32_t reserved;
    ExtentDescriptor metadata;
    uint64_t metadataSectors;
};

#define TEMRIXFS_MAX_FREE_EXTENTS 4096

#define TEMRIXFS_FILE_MAGIC 0x46455854524D5846ULL

struct FileExtentHeader
{
    uint64_t magic;
    uint32_t version;
    uint32_t reserved;
    uint64_t sizeBytes;
    ExtentDescriptor data;
};

class IMetadataBlockIo
{
public:
    ~IMetadataBlockIo() {}
    virtual bool readBlock(uint64_t lba, uint8_t *buf) = 0;
    virtual bool writeBlock(uint64_t lba, const uint8_t *buf) = 0;
};