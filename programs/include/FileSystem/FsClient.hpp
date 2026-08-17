#pragma once
#include "FsShared.hpp"

static inline uint64_t FsToU64(const char *s)
{
    uint64_t val = 0;
    while (*s >= '0' && *s <= '9')
    {
        val = val * 10 + (uint64_t)(*s - '0');
        s++;
    }
    return val;
}

class FsClient
{
public:
    bool init()
    {
        char regBuf[256];

        uint64_t got = 0;
        while ((got = Syscall::Service::Lookup("fs.reg", regBuf, sizeof(regBuf) - 1)) == 0)
            Syscall::Process::Yield();
        regBuf[got] = '\0';

        uint64_t handle = FsToU64(regBuf);

        char pidBuf[256];
        got = 0;
        while ((got = Syscall::Service::Lookup("fs.pid", pidBuf, sizeof(pidBuf) - 1)) == 0)
            Syscall::Process::Yield();
        pidBuf[got] = '\0';
        m_serverPid = (uint32_t)FsToU64(pidBuf);

        Syscall::Memory::SharedMemResult reg;
        if (Syscall::Memory::MapShared(
                handle,
                ::Memory::Read | ::Memory::Write | ::Memory::User,
                &reg) != 0)
        {
            return false;
        }

        m_registry = reinterpret_cast<FsRegistry *>(reg.virtAddr);

        while (__atomic_load_n(&m_registry->ready, __ATOMIC_ACQUIRE) == 0)
            Syscall::Process::Yield();

        String::Print("[fs-client] connected to server registry\n");

        return true;
    }

    bool readFile(const char *path, uint64_t *outVirt, uint32_t *outLen)
    {
        FsStatus status;
        uint64_t handle = 0;
        uint32_t len = 0;

        bool ok = request(FsRequestRead, path, 0, 0, 0, 0, &status, &handle, &len);
        if (!ok || status != FsDone)
        {
            *outVirt = 0;
            *outLen = 0;
            return false;
        }

        Syscall::Memory::SharedMemResult resp;
        if (Syscall::Memory::MapShared(
                handle,
                ::Memory::Read | ::Memory::User,
                &resp) != 0)
        {
            *outVirt = 0;
            *outLen = 0;
            return false;
        }

        *outVirt = resp.virtAddr;
        *outLen = len;
        return true;
    }

    bool statFile(const char *path, uint32_t *outLen)
    {
        FsStatus status;
        uint64_t handle = 0;

        bool ok = request(FsRequestStat, path, 0, 0, 0, 0, &status, &handle, outLen);
        return ok && status == FsDone;
    }

    
    
    bool listDirectory(const char *path, FsDirEntry *out, uint32_t maxEntries, uint32_t *outCount)
    {
        FsStatus status;
        uint64_t handle = 0;
        uint32_t count = 0; 

        bool ok = request(FsRequestListDir, path, 0, 0, 0, 0, &status, &handle, &count);
        if (!ok || status != FsDone)
        {
            *outCount = 0;
            return false;
        }

        Syscall::Memory::SharedMemResult resp;
        if (Syscall::Memory::MapShared(
                handle,
                ::Memory::Read | ::Memory::User,
                &resp) != 0)
        {
            *outCount = 0;
            return false;
        }

        uint32_t n = count < maxEntries ? count : maxEntries;
        FsDirEntry *src = reinterpret_cast<FsDirEntry *>(resp.virtAddr);
        for (uint32_t i = 0; i < n; i++)
            out[i] = src[i];

        *outCount = n;
        return true;
    }

    
    
    
    

    FsStatus createFile(const char *path)
    {
        return requestSimple(FsRequestCreate, path, /*offset=*/0, /*flag=*/0);
    }

    FsStatus createDirectory(const char *path)
    {
        return requestSimple(FsRequestCreate, path, /*offset=*/0, /*flag=*/1);
    }

    FsStatus removeFile(const char *path)
    {
        return requestSimple(FsRequestRemove, path, /*offset=*/0, /*flag=*/0);
    }

    FsStatus truncateFile(const char *path, uint64_t newSize)
    {
        return requestSimple(FsRequestTruncate, path, /*offset=*/newSize, /*flag=*/0);
    }

    FsStatus writeFile(const char *path, uint64_t offset, const uint8_t *data, uint32_t len)
    {
        Syscall::Memory::SharedMemResult req;
        uint64_t pages = ((uint64_t)len + 0xFFF) >> 12;
        if (pages == 0) pages = 1;

        if (Syscall::Memory::CreateShared(
                pages,
                ::Memory::Read | ::Memory::Write | ::Memory::User,
                &req) != 0)
        {
            return FsError;
        }

        Memory::Copy((uint8_t *)req.virtAddr, data, len);

        return requestSimple(FsRequestWrite, path, offset, /*flag=*/0, req.handle, len);
    }

private:
    
    
    FsStatus requestSimple(FsRequestType type, const char *path, uint64_t offset, uint32_t flag,
                            uint64_t requestHandle = 0, uint32_t requestLen = 0)
    {
        FsStatus status;
        uint64_t handle = 0;
        uint32_t len = 0;

        bool ok = request(type, path, offset, flag, requestHandle, requestLen, &status, &handle, &len);
        return ok ? status : FsError;
    }

    bool request(FsRequestType type, const char *path,
                 uint64_t offset, uint32_t flag, uint64_t requestHandle, uint32_t requestLen,
                 FsStatus *outStatus, uint64_t *outHandle, uint32_t *outLen)
    {
        int slot = claimSlot();
        if (slot == -1)
            return false;

        FsSlot &s = m_registry->slots[slot];

        int i = 0;
        while (path[i] && i < FS_MAX_PATH - 1)
        {
            s.path[i] = path[i];
            i++;
        }
        s.path[i] = '\0';
        s.type = type;
        s.callerPid = Syscall::Process::GetId();
        s.offset = offset;
        s.flag = flag;
        s.requestHandle = requestHandle;
        s.requestLen = requestLen;
        s.responseHandle = 0;
        s.responseLen = 0;

        __atomic_store_n(&s.status, (uint32_t)FsPending, __ATOMIC_RELEASE);

        Syscall::Process::Notify(m_serverPid);

        while (__atomic_load_n(&s.status, __ATOMIC_ACQUIRE) == FsPending)
            Syscall::Process::Wait();

        *outStatus = (FsStatus)__atomic_load_n(&s.status, __ATOMIC_ACQUIRE);
        *outHandle = s.responseHandle;
        *outLen = s.responseLen;

        __atomic_store_n(&s.claimed, 0, __ATOMIC_RELEASE);
        return true;
    }

    int claimSlot()
    {
        for (int i = 0; i < FS_MAX_SLOTS; i++)
        {
            uint32_t expected = 0;
            if (__atomic_compare_exchange_n(
                    &m_registry->slots[i].claimed,
                    &expected, 1,
                    false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
            {
                return i;
            }
        }
        return -1;
    }

private:
    FsRegistry *m_registry = nullptr;
    uint32_t m_serverPid = 0;
};