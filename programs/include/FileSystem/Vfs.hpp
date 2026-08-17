#pragma once
#include "FsShared.hpp"
#include "FsBackend.hpp"

class Vfs
{
public:
    static constexpr uint32_t MAX_MOUNTS = 4;

    bool mount(const char *prefix, FsBackendRef backend)
    {
        if (m_count >= MAX_MOUNTS)
            return false;

        uint32_t len = 0;
        while (prefix[len] != '\0' && len < FS_MAX_PATH - 1)
            len++;

        
        if (len > 1 && prefix[len - 1] == '/')
            len--;

        Mount &m = m_mounts[m_count];
        for (uint32_t i = 0; i < len; i++)
            m.prefix[i] = prefix[i];
        m.prefix[len] = '\0';
        m.prefixLen = len;
        m.backend = backend;

        m_count++;
        return true;
    }

    bool stat(const char *path, uint32_t *outSize, bool *outIsDir)
    {
        char rel[FS_MAX_PATH];
        FsBackendRef *b = resolve(path, rel);
        if (!b) return false;
        return b->ops->stat(b->ctx, rel, outSize, outIsDir);
    }

    bool read(const char *path, uint8_t *buf, uint32_t *outSize)
    {
        char rel[FS_MAX_PATH];
        FsBackendRef *b = resolve(path, rel);
        if (!b) return false;
        return b->ops->read(b->ctx, rel, buf, outSize);
    }

    uint32_t listDir(const char *path, FsDirEntry *out, uint32_t maxEntries)
    {
        char rel[FS_MAX_PATH];
        FsBackendRef *b = resolve(path, rel);
        if (!b) return 0;
        return b->ops->listDir(b->ctx, rel, out, maxEntries);
    }

    bool write(const char *path, uint64_t offset, const uint8_t *buf, uint32_t len)
    {
        char rel[FS_MAX_PATH];
        FsBackendRef *b = resolve(path, rel);
        if (!b) return false;
        return b->ops->write(b->ctx, rel, offset, buf, len);
    }

    bool create(const char *path, bool isDirectory)
    {
        char rel[FS_MAX_PATH];
        FsBackendRef *b = resolve(path, rel);
        if (!b) return false;
        return b->ops->create(b->ctx, rel, isDirectory);
    }

    bool remove(const char *path)
    {
        char rel[FS_MAX_PATH];
        FsBackendRef *b = resolve(path, rel);
        if (!b) return false;
        return b->ops->remove(b->ctx, rel);
    }

    bool truncate(const char *path, uint64_t newSize)
    {
        char rel[FS_MAX_PATH];
        FsBackendRef *b = resolve(path, rel);
        if (!b) return false;
        return b->ops->truncate(b->ctx, rel, newSize);
    }
    
    bool isWritable(const char *path)
    {
        char rel[FS_MAX_PATH];
        FsBackendRef *b = resolve(path, rel);
        if (!b) return false;
        return b->ops->isWritable(b->ctx);
    }

private:
    struct Mount
    {
        char prefix[FS_MAX_PATH];
        uint32_t prefixLen;
        FsBackendRef backend;
    };

    FsBackendRef *resolve(const char *path, char *outRel)
    {
        int best = -1;
        uint32_t bestLen = 0;

        for (uint32_t i = 0; i < m_count; i++)
        {
            Mount &m = m_mounts[i];
            if (matchesPrefix(path, m.prefix, m.prefixLen) && m.prefixLen >= bestLen)
            {
                best = (int)i;
                bestLen = m.prefixLen;
            }
        }

        if (best < 0)
            return nullptr;

        Mount &m = m_mounts[best];
        stripPrefix(path, m.prefixLen, outRel);
        return &m.backend;
    }

    static bool matchesPrefix(const char *path, const char *prefix, uint32_t prefixLen)
    {
        if (prefixLen <= 1) 
            return true;

        for (uint32_t i = 0; i < prefixLen; i++)
        {
            if (path[i] != prefix[i])
                return false;
        }

        char next = path[prefixLen];
        return next == '\0' || next == '/';
    }

    static void stripPrefix(const char *path, uint32_t prefixLen, char *outRel)
    {
        if (prefixLen <= 1)
        {
            copyPath(path, outRel); 
            return;
        }

        const char *rest = path + prefixLen;
        if (*rest == '\0')
        {
            outRel[0] = '/';
            outRel[1] = '\0';
            return;
        }

        copyPath(rest, outRel); 
    }

    static void copyPath(const char *src, char *dst)
    {
        uint32_t i = 0;
        while (src[i] != '\0' && i < FS_MAX_PATH - 1)
        {
            dst[i] = src[i];
            i++;
        }
        dst[i] = '\0';
    }

private:
    Mount m_mounts[MAX_MOUNTS];
    uint32_t m_count = 0;
};