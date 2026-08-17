#pragma once
#include "FileSystem/FsShared.hpp"
#include "FileSystem/Vfs.hpp"

class FsServer
{
public:
    explicit FsServer(Vfs &vfs) : m_vfs(vfs) {}

    bool init()
    {
        uint64_t pages = (sizeof(FsRegistry) + 0xFFF) >> 12;

        Syscall::Memory::SharedMemResult reg;
        if (Syscall::Memory::CreateShared(
                pages,
                ::Memory::Read | ::Memory::Write | ::Memory::User,
                &reg) != 0)
        {
            return false;
        }

        m_registry = reinterpret_cast<FsRegistry *>(reg.virtAddr);

        uint8_t *raw = reinterpret_cast<uint8_t *>(m_registry);
        for (uint64_t i = 0; i < sizeof(FsRegistry); i++)
            raw[i] = 0;

        char handleStr[21];
        Syscall::Service::Publish("fs.reg", String::FromU64(reg.handle, handleStr));

        uint32_t myPid = Syscall::Process::GetId();
        char pidStr[21];
        Syscall::Service::Publish("fs.pid", String::FromU64(myPid, pidStr));

        __atomic_store_n(&m_registry->ready, 1, __ATOMIC_RELEASE);

        String::Print("[fs-server] ready\n");

        return true;
    }

    void poll()
    {
        for (int i = 0; i < FS_MAX_SLOTS; i++)
        {
            FsSlot &slot = m_registry->slots[i];

            if (__atomic_load_n(&slot.claimed, __ATOMIC_ACQUIRE) == 0)
                continue;
            if (__atomic_load_n(&slot.status, __ATOMIC_ACQUIRE) != FsPending)
                continue;

            processSlot(slot);
        }
    }

private:
    void processSlot(FsSlot &slot)
    {
        String::Print("[fs-server] got request: ");
        String::Print(slot.path);
        String::Print("\n");

        switch (slot.type)
        {
            case FsRequestStat:     processStat(slot); break;
            case FsRequestListDir:  processListDir(slot); break;
            case FsRequestWrite:    processWrite(slot); break;
            case FsRequestCreate:   processCreate(slot); break;
            case FsRequestRemove:   processRemove(slot); break;
            case FsRequestTruncate: processTruncate(slot); break;
            case FsRequestRead:
            default:                processRead(slot); break;
        }
    }

    void processStat(FsSlot &slot)
    {
        uint32_t size = 0;
        bool isDir = false;
        bool found = m_vfs.stat(slot.path, &size, &isDir);
        finishSlot(slot, found ? FsDone : FsNotFound, 0, size);
    }

    void processListDir(FsSlot &slot)
    {
        static constexpr uint32_t MAX_ENTRIES = 256;
        FsDirEntry entries[MAX_ENTRIES];

        uint32_t count = m_vfs.listDir(slot.path, entries, MAX_ENTRIES);

        uint64_t bytes = (uint64_t)count * sizeof(FsDirEntry);
        uint64_t pages = (bytes + 0xFFF) >> 12;
        if (pages == 0)
            pages = 1;

        Syscall::Memory::SharedMemResult resp;
        if (Syscall::Memory::CreateShared(
                pages,
                ::Memory::Read | ::Memory::Write | ::Memory::User,
                &resp) != 0)
        {
            finishSlot(slot, FsError, 0, 0);
            return;
        }

        Memory::Copy((uint8_t *)resp.virtAddr, (uint8_t *)entries, bytes);

        finishSlot(slot, FsDone, resp.handle, count);
    }

    void processRead(FsSlot &slot)
    {
        uint32_t size = 0;
        bool isDir = false;
        bool found = m_vfs.stat(slot.path, &size, &isDir);

        if (!found)
        {
            finishSlot(slot, FsNotFound, 0, 0);
            return;
        }

        uint64_t respPages = (size + 0xFFF) >> 12;
        if (respPages == 0)
            respPages = 1;

        Syscall::Memory::SharedMemResult resp;
        if (Syscall::Memory::CreateShared(
                respPages,
                ::Memory::Read | ::Memory::Write | ::Memory::User,
                &resp) != 0)
        {
            finishSlot(slot, FsError, 0, 0);
            return;
        }

        uint32_t actualSize = 0;
        bool ok = m_vfs.read(slot.path, reinterpret_cast<uint8_t *>(resp.virtAddr), &actualSize);

        if (!ok)
        {
            finishSlot(slot, FsError, 0, 0);
            return;
        }

        finishSlot(slot, FsDone, resp.handle, actualSize);
    }

    void processWrite(FsSlot &slot)
    {
        if (!m_vfs.isWritable(slot.path))
        {
            finishSlot(slot, FsNotSupported, 0, 0);
            return;
        }

        Syscall::Memory::SharedMemResult req;
        if (Syscall::Memory::MapShared(
                slot.requestHandle,
                ::Memory::Read | ::Memory::User,
                &req) != 0)
        {
            finishSlot(slot, FsError, 0, 0);
            return;
        }

        bool ok = m_vfs.write(slot.path, slot.offset,
                               reinterpret_cast<uint8_t *>(req.virtAddr), slot.requestLen);

        finishSlot(slot, ok ? FsDone : FsError, 0, 0);
    }

    void processCreate(FsSlot &slot)
    {
        if (!m_vfs.isWritable(slot.path))
        {
            finishSlot(slot, FsNotSupported, 0, 0);
            return;
        }

        bool ok = m_vfs.create(slot.path, slot.flag != 0);
        finishSlot(slot, ok ? FsDone : FsError, 0, 0);
    }

    void processRemove(FsSlot &slot)
    {
        if (!m_vfs.isWritable(slot.path))
        {
            finishSlot(slot, FsNotSupported, 0, 0);
            return;
        }

        bool ok = m_vfs.remove(slot.path);
        finishSlot(slot, ok ? FsDone : FsError, 0, 0);
    }

    void processTruncate(FsSlot &slot)
    {
        if (!m_vfs.isWritable(slot.path))
        {
            finishSlot(slot, FsNotSupported, 0, 0);
            return;
        }

        bool ok = m_vfs.truncate(slot.path, slot.offset);
        finishSlot(slot, ok ? FsDone : FsError, 0, 0);
    }

    void finishSlot(FsSlot &slot, FsStatus status, uint64_t handle, uint32_t len)
    {
        slot.responseHandle = handle;
        slot.responseLen = len;

        __atomic_store_n(&slot.status, (uint32_t)status, __ATOMIC_RELEASE);

        Syscall::Process::Notify(slot.callerPid);
    }

private:
    Vfs &m_vfs;
    FsRegistry *m_registry = nullptr;
};