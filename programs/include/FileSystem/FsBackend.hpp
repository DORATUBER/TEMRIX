#pragma once
#include "FsShared.hpp"

struct FsBackendOps
{
    bool (*stat)(void *ctx, const char *path, uint32_t *outSize, bool *outIsDir);
    bool (*read)(void *ctx, const char *path, uint8_t *buf, uint32_t *outSize);
    uint32_t (*listDir)(void *ctx, const char *path, FsDirEntry *out, uint32_t maxEntries);
    bool (*write)(void *ctx, const char *path, uint64_t offset, const uint8_t *buf, uint32_t len);
    bool (*create)(void *ctx, const char *path, bool isDirectory);
    bool (*remove)(void *ctx, const char *path);
    bool (*truncate)(void *ctx, const char *path, uint64_t newSize);
    bool (*isWritable)(void *ctx);
};

struct FsBackendRef
{
    const FsBackendOps *ops;
    void *ctx;
};