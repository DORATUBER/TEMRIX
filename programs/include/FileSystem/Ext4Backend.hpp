#pragma once
#include "FsShared.hpp"
#include "FsBackend.hpp"
#include "FileSystem/Ext4/ext4.hpp"

class Ext4Backend
{
public:
    explicit Ext4Backend(Ext4 &fs) : m_fs(fs) {}

    FsBackendRef ref() { return FsBackendRef{&kOps, this}; }

private:
    bool stat(const char *path, uint32_t *outSize, bool *outIsDir)
    {
        bool found = false;
        m_fs.statOrReadFile(path, nullptr, outSize, &found);
        if (outIsDir)
            *outIsDir = false; 
        return found;
    }

    bool read(const char *path, uint8_t *buf, uint32_t *outSize)
    {
        bool found = false;
        bool ok = m_fs.statOrReadFile(path, buf, outSize, &found);
        return ok && found;
    }

    uint32_t listDir(const char *path, FsDirEntry *out, uint32_t maxEntries)
    {
        return m_fs.listDirectory(path, out, maxEntries);
    }

    bool write(const char *, uint64_t, const uint8_t *, uint32_t) { return false; }
    bool create(const char *, bool) { return false; }
    bool remove(const char *) { return false; }
    bool truncate(const char *, uint64_t) { return false; }
    bool isWritable() const { return false; }

private:
    static bool statT(void *ctx, const char *path, uint32_t *outSize, bool *outIsDir)
    { return static_cast<Ext4Backend *>(ctx)->stat(path, outSize, outIsDir); }

    static bool readT(void *ctx, const char *path, uint8_t *buf, uint32_t *outSize)
    { return static_cast<Ext4Backend *>(ctx)->read(path, buf, outSize); }

    static uint32_t listDirT(void *ctx, const char *path, FsDirEntry *out, uint32_t maxEntries)
    { return static_cast<Ext4Backend *>(ctx)->listDir(path, out, maxEntries); }

    static bool writeT(void *ctx, const char *path, uint64_t offset, const uint8_t *buf, uint32_t len)
    { return static_cast<Ext4Backend *>(ctx)->write(path, offset, buf, len); }

    static bool createT(void *ctx, const char *path, bool isDirectory)
    { return static_cast<Ext4Backend *>(ctx)->create(path, isDirectory); }

    static bool removeT(void *ctx, const char *path)
    { return static_cast<Ext4Backend *>(ctx)->remove(path); }

    static bool truncateT(void *ctx, const char *path, uint64_t newSize)
    { return static_cast<Ext4Backend *>(ctx)->truncate(path, newSize); }

    static bool isWritableT(void *ctx)
    { return static_cast<Ext4Backend *>(ctx)->isWritable(); }

    static constexpr FsBackendOps kOps = {
        &Ext4Backend::statT,
        &Ext4Backend::readT,
        &Ext4Backend::listDirT,
        &Ext4Backend::writeT,
        &Ext4Backend::createT,
        &Ext4Backend::removeT,
        &Ext4Backend::truncateT,
        &Ext4Backend::isWritableT,
    };

    Ext4 &m_fs;
};