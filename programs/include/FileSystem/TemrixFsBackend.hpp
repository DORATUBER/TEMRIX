#pragma once
#include "FsShared.hpp"
#include "FsBackend.hpp"
#include "FileSystem/TemrixFs/trxfs.hpp"

class TemrixFsBackend
{
public:
    explicit TemrixFsBackend(TemrixFs &fs) : m_fs(fs) {}

    FsBackendRef ref() { return FsBackendRef{&kOps, this}; }

    bool stat(const char *path, uint32_t *outSize, bool *outIsDir)
    {
        FileNode node;
        if (!m_fs.resolvePath(path, node))
            return false;
        *outSize = (uint32_t)node.header.sizeBytes;
        if (outIsDir)
            *outIsDir = node.isDirectory;
        return true;
    }

    bool read(const char *path, uint8_t *buf, uint32_t *outSize)
    {
        FileNode node;
        if (!m_fs.resolvePath(path, node) || node.isDirectory)
            return false;

        uint32_t size = (uint32_t)node.header.sizeBytes;
        if (!m_fs.readFileNode(node, 0, buf, size))
            return false;

        *outSize = size;
        return true;
    }

    uint32_t listDir(const char *path, FsDirEntry *out, uint32_t maxEntries)
    {
        FileNode dir;
        if (!m_fs.resolvePath(path, dir) || !dir.isDirectory)
            return 0;

        static constexpr uint32_t SCRATCH = 256;
        DirEntry raw[SCRATCH];
        uint32_t cap = maxEntries < SCRATCH ? maxEntries : SCRATCH;
        uint32_t count = m_fs.listDirectory(dir, raw, cap);

        for (uint32_t i = 0; i < count; i++)
        {
            FsDirEntry &dst = out[i];

            uint32_t n = 0;
            while (raw[i].name[n] != '\0' && n < FS_MAX_NAME - 1)
            {
                dst.name[n] = raw[i].name[n];
                n++;
            }
            dst.name[n] = '\0';

            dst.fileType = (raw[i].type == TEMRIXFS_DIRENT_TYPE_DIR)
                               ? FsTypeDirectory
                               : FsTypeRegular;

            dst.size = 0;
            FileNode child;
            if (m_fs.lookup(dir, raw[i].name, child))
                dst.size = child.header.sizeBytes;
        }

        return count;
    }

    bool write(const char *path, uint64_t offset, const uint8_t *buf, uint32_t len)
    {
        FileNode node;
        if (!m_fs.resolvePath(path, node) || node.isDirectory)
            return false;
        return m_fs.writeFileNode(node, offset, buf, len);
    }

    bool create(const char *path, bool isDirectory)
    {
        FileNode parent, child;
        const char *name = nullptr;
        if (!splitParent(path, parent, name))
            return false;

        return isDirectory ? m_fs.createDirectory(parent, name, child)
                           : m_fs.createRegularFile(parent, name, child);
    }

    bool remove(const char *path)
    {
        FileNode parent;
        const char *name = nullptr;
        if (!splitParent(path, parent, name))
            return false;

        return m_fs.remove(parent, name);
    }

    bool truncate(const char *path, uint64_t newSize)
    {
        FileNode node;
        if (!m_fs.resolvePath(path, node) || node.isDirectory)
            return false;
        return m_fs.truncateFileNode(node, newSize);
    }

    bool isWritable() const { return true; }

private:
    bool splitParent(const char *path, FileNode &outParent, const char *&outName)
    {
        uint32_t len = 0;
        while (path[len] != '\0')
            len++;

        uint32_t end = len;
        while (end > 1 && path[end - 1] == '/')
            end--;

        int32_t lastSlash = -1;
        for (uint32_t i = 0; i < end; i++)
            if (path[i] == '/')
                lastSlash = (int32_t)i;

        if (lastSlash < 0)
            return false;

        outName = path + lastSlash + 1;
        if (*outName == '\0')
            return false;

        static char parentBuf[FS_MAX_PATH];
        if (lastSlash == 0)
        {
            parentBuf[0] = '/';
            parentBuf[1] = '\0';
        }
        else
        {
            uint32_t i = 0;
            for (; i < (uint32_t)lastSlash && i < FS_MAX_PATH - 1; i++)
                parentBuf[i] = path[i];
            parentBuf[i] = '\0';
        }

        return m_fs.resolvePath(parentBuf, outParent) && outParent.isDirectory;
    }

    static bool statT(void *ctx, const char *path, uint32_t *outSize, bool *outIsDir)
    {
        return static_cast<TemrixFsBackend *>(ctx)->stat(path, outSize, outIsDir);
    }

    static bool readT(void *ctx, const char *path, uint8_t *buf, uint32_t *outSize)
    {
        return static_cast<TemrixFsBackend *>(ctx)->read(path, buf, outSize);
    }

    static uint32_t listDirT(void *ctx, const char *path, FsDirEntry *out, uint32_t maxEntries)
    {
        return static_cast<TemrixFsBackend *>(ctx)->listDir(path, out, maxEntries);
    }

    static bool writeT(void *ctx, const char *path, uint64_t offset, const uint8_t *buf, uint32_t len)
    {
        return static_cast<TemrixFsBackend *>(ctx)->write(path, offset, buf, len);
    }

    static bool createT(void *ctx, const char *path, bool isDirectory)
    {
        return static_cast<TemrixFsBackend *>(ctx)->create(path, isDirectory);
    }

    static bool removeT(void *ctx, const char *path)
    {
        return static_cast<TemrixFsBackend *>(ctx)->remove(path);
    }

    static bool truncateT(void *ctx, const char *path, uint64_t newSize)
    {
        return static_cast<TemrixFsBackend *>(ctx)->truncate(path, newSize);
    }

    static bool isWritableT(void *ctx)
    {
        return static_cast<TemrixFsBackend *>(ctx)->isWritable();
    }

    static constexpr FsBackendOps kOps = {
        &TemrixFsBackend::statT,
        &TemrixFsBackend::readT,
        &TemrixFsBackend::listDirT,
        &TemrixFsBackend::writeT,
        &TemrixFsBackend::createT,
        &TemrixFsBackend::removeT,
        &TemrixFsBackend::truncateT,
        &TemrixFsBackend::isWritableT,
    };

private:
    TemrixFs &m_fs;
};