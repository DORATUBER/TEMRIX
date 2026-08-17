#pragma once
#include <temrixstd.h>
#define FS_MAX_SLOTS 8
#define FS_MAX_PATH 4096
#define FS_MAX_NAME 256

enum FsRequestType : uint32_t
{
    FsRequestRead = 0,
    FsRequestStat = 1,
    FsRequestListDir = 2,
    FsRequestWrite = 3,
    FsRequestCreate = 4,
    FsRequestRemove = 5,
    FsRequestTruncate = 6,
};

enum FsStatus : uint32_t
{
    FsPending = 0,
    FsDone = 1,
    FsError = 2,
    FsNotFound = 3,
    FsNotSupported = 4, 
};

enum FsFileType : uint8_t
{
    FsTypeUnknown = 0,
    FsTypeRegular = 1,
    FsTypeDirectory = 2,
};

struct FsDirEntry
{
    char name[FS_MAX_NAME];
    uint8_t fileType;
    uint64_t size;
};

struct FsSlot
{
    uint32_t claimed;
    uint32_t status;
    FsRequestType type;
    char path[FS_MAX_PATH];
    uint64_t requestHandle;
    uint32_t requestLen;
    uint64_t offset;
    uint32_t flag;
    uint64_t responseHandle;
    uint32_t responseLen;

    uint32_t callerPid;
};

struct FsRegistry
{
    uint32_t ready;
    FsSlot slots[FS_MAX_SLOTS];
};