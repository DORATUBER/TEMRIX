#include "syscall.hpp"
#include "KernelState.hpp"
#include "MemoryCommon.hpp"
#include "publish.hpp"
#include "spawn.hpp"
#include "Serial.hpp"
#include "interrupts.hpp"
#include "stdlib/kVector.hpp"

extern Kernel kernel;

struct SharedRegion
{
    uint64_t *pages;
    uint32_t pageCount;
    bool used;
    uint32_t ownerPid;
};

static Memory::KVector<SharedRegion> s_shared;
static bool s_sharedInitialized = false;

static void ensureSharedInit()
{
    if (!s_sharedInitialized)
    {
        s_shared.init(&kernel.allocator);
        s_sharedInitialized = true;
    }
}

static uint32_t allocateSharedSlot()
{
    for (uint32_t i = 0; i < s_shared.size(); i++)
        if (!s_shared[i].used)
            return i;

    SharedRegion empty{};
    if (!s_shared.push(empty))
        return (uint32_t)-1;
    return (uint32_t)(s_shared.size() - 1);
}

static bool isUserPtr(uint64_t ptr, uint64_t size)
{
    if (!ptr)
        return false;
    if (ptr >= USER_SPACE_MAX)
        return false;
    if (size > 0 && ptr + size > USER_SPACE_MAX)
        return false;
    if (size > 0 && ptr + size < ptr)
        return false;
    return true;
}

static bool hasMemory(uint64_t needed)
{
    uint32_t free = kernel.allocator.getFreeMemory();
    if (free < RESERVED_MEMORY)
        return false;
    return needed <= (free - RESERVED_MEMORY);
}

static Memory::PageTableContext *currentPageTable()
{
    Task *t = kernel.scheduler.current();
    if (t && t->pageTable)
        return t->pageTable;
    return &kernel.ptCtx;
}

static bool copyFromUser(uint64_t userPtr, void *dst, uint64_t size)
{
    if (!isUserPtr(userPtr, size))
        return false;
    Memory::PageTableContext *pt = currentPageTable();

    uint64_t copied = 0;
    while (copied < size)
    {
        uint64_t phys = Memory::virtToPhys(pt, userPtr + copied);
        if (!phys)
            return false;
        uint64_t pageOffset = (userPtr + copied) & 0xFFF;
        uint64_t pageRemain = 0x1000 - pageOffset;
        uint64_t toCopy = pageRemain < (size - copied) ? pageRemain : (size - copied);
        uint8_t *src = (uint8_t *)Memory::phys_to_virt(phys - pageOffset) + pageOffset;
        Memory::copy((uint8_t *)dst + copied, src, toCopy);
        copied += toCopy;
    }
    return true;
}

static bool copyToUser(uint64_t userPtr, const void *src, uint64_t size)
{
    if (!isUserPtr(userPtr, size))
        return false;
    Memory::PageTableContext *pt = currentPageTable();

    uint64_t copied = 0;
    while (copied < size)
    {
        uint64_t phys = Memory::virtToPhys(pt, userPtr + copied);
        if (!phys)
            return false;
        uint64_t pageOffset = (userPtr + copied) & 0xFFF;
        uint64_t pageRemain = 0x1000 - pageOffset;
        uint64_t toCopy = pageRemain < (size - copied) ? pageRemain : (size - copied);
        uint8_t *dst = (uint8_t *)Memory::phys_to_virt(phys - pageOffset) + pageOffset;
        Memory::copy(dst, (const uint8_t *)src + copied, toCopy);
        copied += toCopy;
    }
    return true;
}

static uint64_t copyStringFromUser(uint64_t userPtr, char *dst, uint64_t maxLen)
{
    if (!isUserPtr(userPtr, 0))
        return 0;
    Memory::PageTableContext *pt = currentPageTable();

    uint64_t len = 0;
    while (len < maxLen)
    {
        uint64_t phys = Memory::virtToPhys(pt, userPtr + len);
        if (!phys)
            break;
        uint64_t pageOffset = (userPtr + len) & 0xFFF;
        uint64_t pageRemain = 0x1000 - pageOffset;
        char *src = (char *)Memory::phys_to_virt(phys - pageOffset) + pageOffset;
        for (uint64_t i = 0; i < pageRemain && len < maxLen; i++)
        {
            if (src[i] == '\0')
                return len;
            dst[len++] = src[i];
        }
    }
    return len;
}

static bool taskCanMapDevice(Task *t, const PCI::KernelDevice *d, uint32_t deviceIndex)
{
    for (auto &g : t->deviceGrants)
    {
        if (!g.used)
            continue;
        if (g.kind == DeviceGrantKind::AnyDevice)
            return true;
        if (g.kind == DeviceGrantKind::ClassWildcard && g.classCode == d->classCode)
            return true;
        if (g.kind == DeviceGrantKind::SpecificDevice && g.deviceIndex == deviceIndex)
            return true;
    }
    return false;
}

static bool granterCoversGrant(Task *granter, DeviceGrantKind kind, uint64_t param)
{
    for (auto &g : granter->deviceGrants)
    {
        if (!g.used)
            continue;
        if (g.kind == DeviceGrantKind::AnyDevice)
            return true;

        if (kind == DeviceGrantKind::AnyDevice)
            continue;

        if (kind == DeviceGrantKind::ClassWildcard)
        {
            if (g.kind == DeviceGrantKind::ClassWildcard && g.classCode == (uint8_t)param)
                return true;
        }
        else
        {
            uint32_t deviceIndex = (uint32_t)param;
            if (g.kind == DeviceGrantKind::SpecificDevice && g.deviceIndex == deviceIndex)
                return true;
            if (g.kind == DeviceGrantKind::ClassWildcard)
            {
                const PCI::KernelDevice *d = kernel.pci.getDevice(deviceIndex);
                if (d && d->classCode == g.classCode)
                    return true;
            }
        }
    }
    return false;
}

static bool addDeviceGrant(Task *t, DeviceGrantKind kind, uint8_t classCode, uint32_t deviceIndex)
{
    for (auto &g : t->deviceGrants)
    {
        if (!g.used)
        {
            g = {true, kind, classCode, deviceIndex};
            return true;
        }
    }
    return false;
}

static bool taskCanMapShared(Task *t, uint32_t handle)
{
    if (s_shared[handle].ownerPid == t->id)
        return true;
    for (auto &g : t->sharedGrants)
        if (g.used && g.handle == handle)
            return true;
    return false;
}

static bool addSharedGrant(Task *t, uint32_t handle, bool canWrite, uint32_t grantedBy)
{
    for (auto &g : t->sharedGrants)
    {
        if (g.used && g.handle == handle)
        {
            g.canWrite = g.canWrite || canWrite;
            g.grantedBy = grantedBy;   
            return true;
        }
    }
    for (auto &g : t->sharedGrants)
    {
        if (!g.used)
        {
            g = {true, handle, canWrite, grantedBy};
            return true;
        }
    }
    return false;
}

static bool removeSharedGrant(Task *t, uint32_t handle)
{
    for (auto &g : t->sharedGrants)
    {
        if (g.used && g.handle == handle)
        {
            g.used = false;
            return true;
        }
    }
    return false;
}

enum WriteFlags : uint64_t
{
    WriteNone = 0,
    WriteFlush = 1 << 0,
};

static uint64_t syscallWrite(uint64_t bufPtr, uint64_t len, uint64_t flags)
{
    if (len > 0)
    {
        char local[256];
        uint64_t n = (len < sizeof(local) - 1) ? len : sizeof(local) - 1;

        if (!copyFromUser(bufPtr, local, n))
            return (uint64_t)-1;

        local[n] = '\0';
        Serial::print(local);
    }

    if (flags & WriteFlush)
        Serial::render();

    return len;
}

static uint64_t syscallGetMemoryMap(uint64_t bufPtr, uint64_t maxEntries)
{
    Task *t = kernel.scheduler.current();
    if (!t || !t->userVmm)
        return (uint64_t)-1;

    if (bufPtr == 0 || maxEntries == 0)
        return t->userVmm->vmaCount();

    if (maxEntries > 4096)
        return (uint64_t)-1;
    if (!isUserPtr(bufPtr, maxEntries * sizeof(Memory::VMA)))
        return (uint64_t)-1;

    Memory::VMA *tmp = (Memory::VMA *)kernel.slab.malloc(maxEntries * sizeof(Memory::VMA));
    if (!tmp)
        return (uint64_t)-1;

    uint64_t total = t->userVmm->snapshot(tmp, maxEntries);
    uint64_t toCopy = (total < maxEntries) ? total : maxEntries;
    bool ok = copyToUser(bufPtr, tmp, toCopy * sizeof(Memory::VMA));
    kernel.slab.free(tmp);

    return ok ? total : (uint64_t)-1;
}

static uint64_t syscallMmap(uint64_t addr, uint64_t size, uint64_t flags)
{
    if (addr == 0 || size == 0)
        return 0;
    if (addr & 0xFFF)
        return 0;
    if (!isUserPtr(addr, size))
        return 0;
    if (size > MAX_MMAP_SIZE)
        return 0;

    Task *t = kernel.scheduler.current();
    if (!t || !t->userVmm || !t->pageTable)
        return 0;

    void *virt = t->userVmm->allocAt(addr, size, flags, /*zero=*/true);
    return (uint64_t)virt;
}

static uint64_t syscallMunmap(uint64_t virtAddr, uint64_t size)
{
    if (!isUserPtr(virtAddr, size))
        return (uint64_t)-1;

    Task *t = kernel.scheduler.current();
    if (!t || !t->userVmm || !t->pageTable)
        return (uint64_t)-1;

    t->userVmm->free((void *)virtAddr, size);
    return 0;
}

static uint64_t syscallAllocDma(uint64_t addr, uint64_t size, uint64_t flags, uint64_t outPtr)
{
    if (addr == 0 || size == 0 || (addr & 0xFFF))
        return (uint64_t)-1;
    if (!isUserPtr(addr, size))
        return (uint64_t)-1;

    Task *t = kernel.scheduler.current();
    if (!t || !t->userVmm)
        return (uint64_t)-1;

    uint64_t phys = 0;
    void *virt = t->userVmm->allocContigAt(addr, size, flags, &phys);
    if (!virt)
        return (uint64_t)-1;

    uint64_t aligned = (size + 0xFFFULL) & ~0xFFFULL;

    DmaAllocResult result;
    result.virt = (uint64_t)virt;
    result.phys = phys;
    result.size = aligned;

    if (!copyToUser(outPtr, &result, sizeof(result)))
        return (uint64_t)-1;

    return 0;
}

static uint64_t syscallMapBar(uint64_t addr, uint64_t deviceIndex, uint64_t barIndex, uint64_t flags)
{
    const PCI::KernelDevice *d = kernel.pci.getDevice((uint32_t)deviceIndex);
    if (!d || barIndex >= 6)
    {
        Serial::printf("[mapbar] invalid device/bar: idx=%llu bar=%llu\n", deviceIndex, barIndex);
        Serial::render();
        return 0;
    }

    Task *t = kernel.scheduler.current();
    if (!t || !t->userVmm)
        return 0;

    if (!taskCanMapDevice(t, d, (uint32_t)deviceIndex))
    {
        Serial::printf("[mapbar] pid=%u DENIED device idx=%llu (bus=%u dev=%u func=%u vendor=0x%04x device=0x%04x)\n",
                       t->id, deviceIndex, d->bus, d->dev, d->func, d->vendorId, d->deviceId);
        Serial::render();
        return 0;
    }

    uint64_t phys = d->bars[barIndex];
    uint64_t size = d->barSizes[barIndex];
    if (phys == 0 || size == 0)
    {
        Serial::printf("[mapbar] pid=%u device idx=%llu bar%llu unpopulated (phys=0x%llx size=0x%llx)\n",
                       t->id, deviceIndex, barIndex, phys, size);
        Serial::render();
        return 0;
    }

    if (addr == 0 || (addr & 0xFFF) || !isUserPtr(addr, size))
    {
        Serial::printf("[mapbar] pid=%u bad requested addr=0x%llx\n", t->id, addr);
        Serial::render();
        return 0;
    }

    void *virt = t->userVmm->allocMmioAt(addr, phys, size, flags);
    if (!virt)
    {
        Serial::printf("[mapbar] pid=%u allocMmioAt failed for device idx=%llu bar%llu (addr taken?)\n",
                       t->id, deviceIndex, barIndex);
        Serial::render();
    }
    return (uint64_t)virt;
}

static uint64_t syscallUnmapBar(uint64_t virtAddr, uint64_t size)
{
    if (virtAddr == 0 || size == 0)
        return (uint64_t)-1;

    Task *t = kernel.scheduler.current();
    if (!t || !t->userVmm)
        return (uint64_t)-1;

    t->userVmm->freeMmio((void *)virtAddr, size);
    return 0;
}

static uint64_t syscallCreateShared(uint64_t addr, uint64_t pageCount, uint64_t flags, uint64_t outPtr)
{
    if (pageCount == 0)
        return (uint64_t)-1;
    if (addr == 0 || (addr & 0xFFF))
        return (uint64_t)-1;
    if (!isUserPtr(addr, pageCount * 0x1000))
        return (uint64_t)-1;

    Task *t = kernel.scheduler.current();
    if (!t || !t->userVmm)
        return (uint64_t)-1;

    uint64_t needed = pageCount * 0x1000 + pageCount * sizeof(uint64_t);
    if (!hasMemory(needed))
        return (uint64_t)-1;

    ensureSharedInit();

    uint32_t handle = allocateSharedSlot();
    if (handle == (uint32_t)-1)
        return (uint64_t)-1;

    SharedRegion &r = s_shared[handle];

    r.pages = (uint64_t *)kernel.allocator.malloc(pageCount * sizeof(uint64_t));
    if (!r.pages)
        return (uint64_t)-1;

    r.pageCount = (uint32_t)pageCount;
    r.used = true;
    r.ownerPid = t->id;

    for (uint32_t i = 0; i < pageCount; i++)
    {
        void *pv = kernel.allocator.malloc(0x1000);
        if (!pv)
        {
            for (uint32_t j = 0; j < i; j++)
                kernel.allocator.free((void *)Memory::phys_to_virt(r.pages[j]));
            kernel.allocator.free(r.pages);
            r.pages = nullptr;
            r.used = false;
            return (uint64_t)-1;
        }
        Memory::set(pv, 0, 0x1000);
        r.pages[i] = Memory::virt_to_phys((uint64_t)pv);
    }

    void *virt = t->userVmm->mapSharedAt(addr, r.pages, pageCount, flags);
    if (!virt)
    {
        for (uint32_t i = 0; i < pageCount; i++)
            kernel.allocator.free((void *)Memory::phys_to_virt(r.pages[i]));
        kernel.allocator.free(r.pages);
        r.pages = nullptr;
        r.used = false;
        return (uint64_t)-1;
    }

    SharedMemoryResult result;
    result.handle = handle;
    result.virtAddr = (uint64_t)virt;
    result.pageCount = pageCount;

    if (!copyToUser(outPtr, &result, sizeof(result)))
    {
        t->userVmm->unmapShared(virt, pageCount);
        for (uint32_t i = 0; i < pageCount; i++)
            kernel.allocator.free((void *)Memory::phys_to_virt(r.pages[i]));
        kernel.allocator.free(r.pages);
        r.pages = nullptr;
        r.used = false;
        return (uint64_t)-1;
    }

    return 0;
}

static uint64_t syscallMapShared(uint64_t addr, uint64_t handle, uint64_t flags, uint64_t outPtr)
{
    if (handle >= s_shared.size() || !s_shared[handle].used)
        return (uint64_t)-1;
    if (addr == 0 || (addr & 0xFFF))
        return (uint64_t)-1;

    Task *t = kernel.scheduler.current();
    if (!t || !t->userVmm)
        return (uint64_t)-1;

    SharedRegion &r = s_shared[handle];
    if (!isUserPtr(addr, (uint64_t)r.pageCount * 0x1000))
        return (uint64_t)-1;

    // if (!taskCanMapShared(t, (uint32_t)handle)) return (uint64_t)-1;

    void *virt = t->userVmm->mapSharedAt(addr, r.pages, r.pageCount, flags);
    if (!virt)
        return (uint64_t)-1;

    SharedMemoryResult result;
    result.handle = handle;
    result.virtAddr = (uint64_t)virt;
    result.pageCount = r.pageCount;

    if (!copyToUser(outPtr, &result, sizeof(result)))
    {
        t->userVmm->unmapShared(virt, r.pageCount);
        return (uint64_t)-1;
    }

    return 0;
}

static uint64_t syscallUnmapShared(uint64_t virtAddr, uint64_t pageCount)
{
    if (!isUserPtr(virtAddr, pageCount * 0x1000))
        return (uint64_t)-1;

    Task *t = kernel.scheduler.current();
    if (!t || !t->userVmm)
        return (uint64_t)-1;

    t->userVmm->unmapShared((void *)virtAddr, pageCount);
    return 0;
}

static uint64_t syscallDestroyShared(uint64_t handle)
{
    if (handle >= s_shared.size() || !s_shared[handle].used)
        return (uint64_t)-1;

    Task *t = kernel.scheduler.current();
    if (!t)
        return (uint64_t)-1;

    SharedRegion &r = s_shared[handle];
    if (r.ownerPid != t->id)
        return (uint64_t)-1;

    for (uint32_t i = 0; i < r.pageCount; i++)
        kernel.allocator.free((void *)Memory::phys_to_virt(r.pages[i]));
    kernel.allocator.free(r.pages);
    r.pages = nullptr;
    r.pageCount = 0;
    r.used = false;
    r.ownerPid = 0;
    return 0;
}

static uint64_t syscallPciGetDevices(uint64_t bufPtr, uint64_t maxCount)
{
    uint32_t total = kernel.pci.getDeviceCount();

    if (bufPtr == 0)
        return total;

    uint32_t toCopy = (maxCount < total) ? (uint32_t)maxCount : total;

    if (!isUserPtr(bufPtr, toCopy * sizeof(PCI::KernelDevice)))
        return (uint64_t)-1;

    for (uint32_t i = 0; i < toCopy; i++)
    {
        const PCI::KernelDevice *d = kernel.pci.getDevice(i);
        if (!copyToUser(bufPtr + i * sizeof(PCI::KernelDevice), d, sizeof(PCI::KernelDevice)))
            return (uint64_t)-1;
    }

    return toCopy;
}

static uint64_t syscallPublish(uint64_t namePtr, uint64_t nameLen, uint64_t dataPtr, uint64_t dataLen)
{
    if (!isUserPtr(namePtr, nameLen) || !isUserPtr(dataPtr, dataLen))
        return 0;

    char name[256], data[256];
    if (nameLen >= sizeof(name) || dataLen >= sizeof(data))
        return 0;

    if (!copyFromUser(namePtr, name, nameLen) || !copyFromUser(dataPtr, data, dataLen))
        return 0;

    name[nameLen] = '\0';
    data[dataLen] = '\0';

    bool ok = kernel.publishTable.publish(name, (uint32_t)nameLen, data, (uint32_t)dataLen);
    return ok ? 1 : 0;
}

static uint64_t syscallLookup(uint64_t namePtr, uint64_t nameLen, uint64_t outPtr, uint64_t outCapacity)
{
    if (!isUserPtr(namePtr, nameLen) || !isUserPtr(outPtr, outCapacity))
        return 0;

    char name[256];
    if (nameLen >= sizeof(name))
        return 0;

    if (!copyFromUser(namePtr, name, nameLen))
        return 0;

    char tmp[256];
    if (outCapacity >= sizeof(tmp))
        return 0;

    uint32_t got = kernel.publishTable.lookup(name, (uint32_t)nameLen, tmp, (uint32_t)outCapacity);
    if (got == 0)
        return 0;

    if (!copyToUser(outPtr, tmp, got))
        return 0;

    return got;
}

static uint64_t syscallYield(SyscallFrame *f)
{
    // just invoke the scheduler as if a timer fired
    uint64_t new_rsp = kernel.scheduler.schedule((uint64_t)f);
    Task *next = kernel.scheduler.current();
    if (next && next->pageTable)
        asm volatile("mov %0, %%cr3" ::"r"(Memory::virt_to_phys((uint64_t)next->pageTable->pml4)) : "memory");
    else
        asm volatile("mov %0, %%cr3" ::"r"(Memory::virt_to_phys((uint64_t)kernel.ptCtx.pml4)) : "memory");
    return new_rsp;
}

static uint64_t syscallExit(SyscallFrame *f)
{
    kernel.scheduler.current()->state = Task::State::PendingDelete;
    uint64_t new_rsp = kernel.scheduler.schedule((uint64_t)f);
    Task *next = kernel.scheduler.current();
    if (next && next->pageTable)
        asm volatile("mov %0, %%cr3" ::"r"(Memory::virt_to_phys((uint64_t)next->pageTable->pml4)) : "memory");
    else
        asm volatile("mov %0, %%cr3" ::"r"(Memory::virt_to_phys((uint64_t)kernel.ptCtx.pml4)) : "memory");
    return new_rsp;
}

static uint64_t syscallWait(SyscallFrame *f)
{
    Task *t = kernel.scheduler.current();
    if (t->notifyPending)
    {
        t->notifyPending = false;
        return (uint64_t)f;
    }
    t->state = Task::State::Waiting;
    uint64_t new_rsp = kernel.scheduler.schedule((uint64_t)f);
    Task *next = kernel.scheduler.current();
    if (next && next->pageTable)
        asm volatile("mov %0, %%cr3" ::"r"(Memory::virt_to_phys((uint64_t)next->pageTable->pml4)) : "memory");
    else
        asm volatile("mov %0, %%cr3" ::"r"(Memory::virt_to_phys((uint64_t)kernel.ptCtx.pml4)) : "memory");
    return new_rsp;
}

static uint64_t syscallNotify(uint64_t taskId, SyscallFrame *f, uint64_t *outRsp)
{
    bool ok = false;
    uint64_t new_rsp = kernel.scheduler.notifyTask((uint32_t)taskId, (uint64_t)f, &ok);

    if (!ok)
    {
        *outRsp = (uint64_t)f; // failure: just resume the caller, nothing to switch to
        return (uint64_t)-1;
    }

    Task *next = kernel.scheduler.current();
    if (next && next->pageTable)
        asm volatile("mov %0, %%cr3" ::"r"(Memory::virt_to_phys((uint64_t)next->pageTable->pml4)) : "memory");
    else
        asm volatile("mov %0, %%cr3" ::"r"(Memory::virt_to_phys((uint64_t)kernel.ptCtx.pml4)) : "memory");

    *outRsp = new_rsp;
    return 0;
}

static uint64_t syscallGetPid(SyscallFrame *f)
{
    return (uint64_t)kernel.scheduler.current()->id;
}

static uint64_t maskMapFlags(uint64_t requested)
{
    return (requested & Memory::PAGE_WRITABLE) | Memory::PAGE_USER;
}

static uint64_t toVmFlags(uint64_t rawPageFlags)
{
    uint64_t f = Memory::VM_READ | Memory::VM_USER;
    if (rawPageFlags & Memory::PAGE_WRITABLE)
        f |= Memory::VM_WRITE;
    return f;
}

static bool coveredByMap(uint64_t addr, uint64_t size, MapDescriptor *maps, uint32_t numMaps)
{
    uint64_t end = addr + size;
    for (uint32_t i = 0; i < numMaps; i++)
    {
        uint64_t mStart = maps[i].dst;
        uint64_t mEnd = mStart + maps[i].pageCount * 0x1000;
        if (addr >= mStart && end <= mEnd)
            return true;
    }
    return false;
}

static bool validateMaps(MapDescriptor *maps, uint32_t numMaps, uint64_t &totalPages)
{
    totalPages = 0;
    for (uint32_t i = 0; i < numMaps; i++)
    {
        MapDescriptor &m = maps[i];
        if (m.pageCount == 0)
            return false;
        if (m.dst & 0xFFF)
            return false;
        if (m.dst >= USER_SPACE_MAX)
            return false;

        uint64_t end = m.dst + m.pageCount * 0x1000;
        if (end > USER_SPACE_MAX || end < m.dst)
            return false;

        for (uint32_t j = 0; j < i; j++)
        {
            uint64_t oStart = maps[j].dst, oEnd = oStart + maps[j].pageCount * 0x1000;
            if (m.dst < oEnd && end > oStart)
                return false;
        }
        totalPages += m.pageCount;
    }
    return true;
}

static bool validateCopies(CopyDescriptor *copies, uint32_t numCopies,
                           MapDescriptor *maps, uint32_t numMaps)
{
    for (uint32_t i = 0; i < numCopies; i++)
    {
        CopyDescriptor &c = copies[i];
        if (c.size == 0)
            return false;
        if (!isUserPtr(c.src, c.size))
            return false;
        uint64_t end = c.dst + c.size;
        if (end < c.dst)
            return false;
        if (!coveredByMap(c.dst, c.size, maps, numMaps))
            return false;
    }
    return true;
}

static bool validateRegisters(RegisterValue *regs, uint32_t numRegs)
{
    for (uint32_t i = 0; i < numRegs; i++)
        if (regs[i].regIndex >= (uint8_t)SpawnReg::Count)
            return false;
    return true;
}

struct SpawnBuffers
{
    MapDescriptor *maps = nullptr;
    CopyDescriptor *copies = nullptr;
    RegisterValue *regs = nullptr;

    void freeAll()
    {
        if (maps)
            kernel.slab.free(maps);
        if (copies)
            kernel.slab.free(copies);
        if (regs)
            kernel.slab.free(regs);
        maps = nullptr;
        copies = nullptr;
        regs = nullptr;
    }
};

static bool snapshotSpawnInfo(uint64_t userInfoPtr, SpawnInfo &info, SpawnBuffers &buf)
{
    if (!copyFromUser(userInfoPtr, &info, sizeof(SpawnInfo)))
        return false;

    if (info.numMaps > MAX_SPAWN_MAPS ||
        info.numCopies > MAX_SPAWN_COPIES ||
        info.numRegisters > (uint32_t)SpawnReg::Count)
        return false;

    if (info.numMaps)
    {
        buf.maps = (MapDescriptor *)kernel.slab.malloc(info.numMaps * sizeof(MapDescriptor));
        if (!buf.maps)
            return false;
        if (!copyFromUser((uint64_t)info.maps, buf.maps, info.numMaps * sizeof(MapDescriptor)))
            return false;
    }
    if (info.numCopies)
    {
        buf.copies = (CopyDescriptor *)kernel.slab.malloc(info.numCopies * sizeof(CopyDescriptor));
        if (!buf.copies)
            return false;
        if (!copyFromUser((uint64_t)info.copies, buf.copies, info.numCopies * sizeof(CopyDescriptor)))
            return false;
    }
    if (info.numRegisters)
    {
        buf.regs = (RegisterValue *)kernel.slab.malloc(info.numRegisters * sizeof(RegisterValue));
        if (!buf.regs)
            return false;
        if (!copyFromUser((uint64_t)info.registers, buf.regs, info.numRegisters * sizeof(RegisterValue)))
            return false;
    }
    return true;
}

static InitialRegisters resolveInitialRegisters(RegisterValue *regs, uint32_t numRegs)
{
    InitialRegisters out;
    for (uint32_t i = 0; i < numRegs; i++)
    {
        switch ((SpawnReg)regs[i].regIndex)
        {
        case SpawnReg::RDI:
            out.rdi = regs[i].value;
            break;
        case SpawnReg::RSI:
            out.rsi = regs[i].value;
            break;
        case SpawnReg::RDX:
            out.rdx = regs[i].value;
            break;
        case SpawnReg::RCX:
            out.rcx = regs[i].value;
            break;
        case SpawnReg::R8:
            out.r8 = regs[i].value;
            break;
        case SpawnReg::R9:
            out.r9 = regs[i].value;
            break;
        case SpawnReg::RBX:
            out.rbx = regs[i].value;
            break;
        case SpawnReg::RBP:
            out.rbp = regs[i].value;
            break;
        case SpawnReg::R12:
            out.r12 = regs[i].value;
            break;
        case SpawnReg::R13:
            out.r13 = regs[i].value;
            break;
        case SpawnReg::R14:
            out.r14 = regs[i].value;
            break;
        case SpawnReg::R15:
            out.r15 = regs[i].value;
            break;
        default:
            break;
        }
    }
    return out;
}

static bool executeMaps(Memory::VMM *childVmm, MapDescriptor *maps, uint32_t numMaps)
{
    for (uint32_t i = 0; i < numMaps; i++)
    {
        MapDescriptor &m = maps[i];
        if (!childVmm->allocAt(m.dst, m.pageCount * 0x1000, toVmFlags(maskMapFlags(m.flags)), m.zero))
            return false;
    }
    return true;
}

static bool copyBetweenPageTables(Memory::PageTableContext *srcPt, Memory::PageTableContext *dstPt,
                                  uint64_t src, uint64_t dst, uint64_t size)
{
    while (size)
    {
        uint64_t srcOff = src & 0xFFF, dstOff = dst & 0xFFF;
        uint64_t chunk = size;
        if (0x1000 - srcOff < chunk)
            chunk = 0x1000 - srcOff;
        if (0x1000 - dstOff < chunk)
            chunk = 0x1000 - dstOff;

        uint64_t srcPhys = Memory::virtToPhys(srcPt, src);
        uint64_t dstPhys = Memory::virtToPhys(dstPt, dst);
        if (!srcPhys || !dstPhys)
            return false;

        uint8_t *s = (uint8_t *)Memory::phys_to_virt(srcPhys - srcOff) + srcOff;
        uint8_t *d = (uint8_t *)Memory::phys_to_virt(dstPhys - dstOff) + dstOff;
        Memory::copy(d, s, chunk);

        src += chunk;
        dst += chunk;
        size -= chunk;
    }
    return true;
}

static bool executeCopies(Memory::PageTableContext *callerPt, Memory::PageTableContext *childPt,
                          CopyDescriptor *copies, uint32_t numCopies)
{
    for (uint32_t i = 0; i < numCopies; i++)
    {
        CopyDescriptor &c = copies[i];
        if (!copyBetweenPageTables(callerPt, childPt, c.src, c.dst, c.size))
            return false;
    }
    return true;
}

static void destroyPageTable(Memory::PageTableContext *pt)
{
    Memory::freePageTable(pt, &kernel.allocator);
    delete pt;
}

static uint64_t syscallSpawn(uint64_t arg1)
{
    Serial::print("[spawn] step 0: enter\n");

    if (!isUserPtr(arg1, sizeof(SpawnInfo)))
    {
        Serial::print("[spawn] bad info ptr\n");
        return (uint64_t)-1;
    }

    SpawnInfo info{};
    SpawnBuffers buf;
    if (!snapshotSpawnInfo(arg1, info, buf))
    {
        Serial::print("[spawn] snapshot failed\n");
        buf.freeAll();
        return (uint64_t)-1;
    }
    Serial::print("[spawn] step 1: snapshot ok\n"); 

    Task *caller = kernel.scheduler.current();

    uint64_t childCaps = info.requestedCapabilities & caller->capabilities;

    if (info.requestDeviceGrant)
    {
        if (info.deviceGrantKind > (uint8_t)DeviceGrantKind::AnyDevice)
        {
            Serial::print("[spawn] bad device grant kind\n");
            buf.freeAll();
            return (uint64_t)-1;
        }

        if (!granterCoversGrant(caller, (DeviceGrantKind)info.deviceGrantKind, info.deviceGrantParam))
        {
            Serial::print("[spawn] device grant not covered by caller\n");
            buf.freeAll();
            return (uint64_t)-1;
        }
    }
    Serial::print("[spawn] step 2: device grant check ok\n"); 

    uint64_t totalPages = 0;
    if (!validateMaps(buf.maps, info.numMaps, totalPages))
    {
        Serial::print("[spawn] validateMaps failed\n");
        buf.freeAll();
        return (uint64_t)-1;
    }
    Serial::print("[spawn] step 3: validateMaps ok\n"); 

    if (!validateCopies(buf.copies, info.numCopies, buf.maps, info.numMaps))
    {
        Serial::print("[spawn] validateCopies failed\n");
        buf.freeAll();
        return (uint64_t)-1;
    }
    Serial::print("[spawn] step 4: validateCopies ok\n"); 

    if (!validateRegisters(buf.regs, info.numRegisters))
    {
        Serial::print("[spawn] validateRegisters failed\n");
        buf.freeAll();
        return (uint64_t)-1;
    }
    Serial::print("[spawn] step 5: validateRegisters ok\n"); 

    Serial::printf("[spawn] entry=0x%llx (USER_SPACE_MAX=0x%llx) stackPointer=0x%llx\n",
                   info.entry, USER_SPACE_MAX, info.stackPointer);

    if (info.entry >= USER_SPACE_MAX)
    {
        Serial::print("[spawn] bad entry\n");
        buf.freeAll();
        return (uint64_t)-1;
    }
    if (!coveredByMap(info.stackPointer & ~0xFFFULL, 0x1000, buf.maps, info.numMaps))
    {
        Serial::print("[spawn] stack ptr not covered\n");
        buf.freeAll();
        return (uint64_t)-1;
    }
    Serial::print("[spawn] step 6: entry/stack checks ok\n"); 

    uint64_t totalBytes = totalPages * 0x1000;
    if (totalBytes > MAX_MMAP_SIZE)
    {
        Serial::print("[spawn] exceeds MAX_MMAP_SIZE\n");
        buf.freeAll();
        return (uint64_t)-1;
    }
    if (!hasMemory(totalBytes))
    {
        Serial::printf("[spawn] hasMemory failed, need=%llu free=%u\n", totalBytes, kernel.allocator.getFreeMemory());
        buf.freeAll();
        return (uint64_t)-1;
    }
    Serial::print("[spawn] step 7: memory checks ok\n"); 

    auto *pt = new Memory::PageTableContext(Memory::createProcessPageTable(&kernel.ptCtx, &kernel.allocator));
    if (!pt)
    {
        Serial::print("[spawn] page table alloc failed\n");
        buf.freeAll();
        return (uint64_t)-1;
    }
    Serial::printf("[spawn] step 8: createProcessPageTable ok pt=%p pml4=%p\n", (void*)pt, (void*)pt->pml4);

    Task *t = Task::createUserShell(pt);
    if (!t)
    {
        Serial::print("[spawn] createUserShell failed\n");
        destroyPageTable(pt);
        buf.freeAll();
        return (uint64_t)-1;
    }
    Serial::printf("[spawn] step 9: createUserShell ok t=%p id=%u\n", (void*)t, t->id);

    if (!executeMaps(t->userVmm, buf.maps, info.numMaps))
    {
        Serial::print("[spawn] executeMaps failed\n");
        t->destroy();
        buf.freeAll();
        return (uint64_t)-1;
    }
    Serial::print("[spawn] step 10: executeMaps ok\n");

    if (!executeCopies(caller->pageTable, pt, buf.copies, info.numCopies))
    {
        Serial::print("[spawn] executeCopies failed\n");
        t->destroy();
        buf.freeAll();
        return (uint64_t)-1;
    }
    Serial::print("[spawn] step 11: executeCopies ok\n");

    if (!Task::finalizeUser(t, info.entry, info.stackPointer, resolveInitialRegisters(buf.regs, info.numRegisters)))
    {
        Serial::print("[spawn] finalizeUser failed\n");
        t->destroy();
        buf.freeAll();
        return (uint64_t)-1;
    }
    Serial::print("[spawn] step 12: finalizeUser ok\n"); 

    buf.freeAll();

    t->capabilities = childCaps;
    if (info.requestDeviceGrant)
    {
        uint8_t classCode = (info.deviceGrantKind == (uint8_t)DeviceGrantKind::ClassWildcard) ? (uint8_t)info.deviceGrantParam : 0;
        uint32_t deviceIdx = (info.deviceGrantKind == (uint8_t)DeviceGrantKind::SpecificDevice) ? (uint32_t)info.deviceGrantParam : 0;
        addDeviceGrant(t, (DeviceGrantKind)info.deviceGrantKind, classCode, deviceIdx);
    }
    Serial::print("[spawn] step 13: device grant added\n"); 

    if (!kernel.scheduler.addTask(t))
    {
        Serial::print("[spawn] addTask failed (MAX_TASKS?)\n");
        t->destroy();
        return (uint64_t)-1;
    }
    Serial::print("[spawn] step 14: addTask ok, returning\n"); 

    return (uint64_t)t->id;
}

static uint64_t syscallGetInfo(uint64_t infoType, uint64_t param, uint64_t outPtr)
{
    switch (infoType)
    {
    case InfoFramebuffer:
    {
        if (!copyToUser(outPtr, &kernel.framebuffer, sizeof(FramebufferInfo)))
            return (uint64_t)-1;
        return 0;
    }
    case InfoPciDevice:
    {
        const PCI::KernelDevice *d = kernel.pci.getDevice((uint32_t)param);
        if (!d)
            return (uint64_t)-1;
        if (!copyToUser(outPtr, d, sizeof(PCI::KernelDevice)))
            return (uint64_t)-1;
        return 0;
    }
    case InfoSharedRegion:
    {
        if (param >= s_shared.size() || !s_shared[param].used)
            return (uint64_t)-1;
        uint32_t pageCount = s_shared[param].pageCount;
        if (!copyToUser(outPtr, &pageCount, sizeof(pageCount)))
            return (uint64_t)-1;
        return 0;
    }
    default:
        return (uint64_t)-1;
    }
}

static uint64_t syscallSubscribeIrq(uint64_t vector)
{
    if (vector > 255)
        return (uint64_t)-1;

    Task *t = kernel.scheduler.current();
    if (!t)
        return (uint64_t)-1;

    for (auto &w : Hardware::g_irqWaiters)
    {
        if (w.used && w.task == t && w.vector == (uint8_t)vector)
        {
            w.enabled = true;
            return 0;
        }
    }

    for (auto &w : Hardware::g_irqWaiters)
    {
        if (!w.used)
        {
            w.vector = (uint8_t)vector;
            w.task = t;
            w.enabled = true;
            w.used = true;
            return 0;
        }
    }

    return (uint64_t)-1;
}

static uint64_t syscallAllocVectors(uint64_t count, uint64_t outPtr)
{
    if (count == 0 || count > 255)
        return (uint64_t)-1;

    Hardware::VectorAllocation alloc =
        kernel.interruptController.allocVectors((uint32_t)count);

    if (alloc.rangeCount == 0)
        return (uint64_t)-1;

    VectorAllocResult result = {};
    result.rangeCount = alloc.rangeCount;
    result.base0 = alloc.ranges[0].base;
    result.count0 = alloc.ranges[0].count;
    if (alloc.rangeCount > 1)
    {
        result.base1 = alloc.ranges[1].base;
        result.count1 = alloc.ranges[1].count;
    }

    if (!copyToUser(outPtr, &result, sizeof(result)))
    {
        kernel.interruptController.freeVectors(alloc);
        return (uint64_t)-1;
    }

    return 0;
}

static void ensureBarMapped(const PCI::KernelDevice *d, uint8_t barIndex)
{
    uint64_t phys = d->bars[barIndex];
    uint64_t size = d->barSizes[barIndex];
    if (phys == 0 || size == 0)
        return;

    uint64_t start = phys & ~0xFFFULL;
    uint64_t end = (phys + size + 0xFFF) & ~0xFFFULL;

    for (uint64_t p = start; p < end; p += 0x1000)
    {
        Memory::mapPage4KB(&kernel.ptCtx,
                           Memory::phys_to_virt(p),
                           p,
                           Memory::PAGE_WRITABLE | Memory::PAGE_PCD | Memory::PAGE_PWT);
    }
}

static uint64_t syscallPciMsixInfo(uint64_t deviceIndex, uint64_t outPtr)
{
    const PCI::KernelDevice *d = kernel.pci.getDevice((uint32_t)deviceIndex);
    if (!d)
        return (uint64_t)-1;

    for (int i = 0; i < 6; i++)
        ensureBarMapped(d, i);

    MsixInfo info = {};

    uint8_t capOffset;
    if (kernel.pci.findMSIX(d->bus, d->dev, d->func, &capOffset))
    {
        uint64_t bars[6];
        for (int i = 0; i < 6; i++)
            bars[i] = d->bars[i];

        PCI::MSIXTable table = kernel.pci.getMSIXTable(d->bus, d->dev, d->func, capOffset, bars);
        info.supported = 1;
        info.count = table.count;
    }

    if (!copyToUser(outPtr, &info, sizeof(info)))
        return (uint64_t)-1;
    return 0;
}

static uint64_t syscallPciMsixEnable(uint64_t deviceIndex, uint64_t entry, uint64_t vector)
{
    if (vector > 255)
        return (uint64_t)-1;

    const PCI::KernelDevice *d = kernel.pci.getDevice((uint32_t)deviceIndex);
    if (!d)
        return (uint64_t)-1;

    for (int i = 0; i < 6; i++)
        ensureBarMapped(d, i);

    uint8_t capOffset;
    if (!kernel.pci.findMSIX(d->bus, d->dev, d->func, &capOffset))
        return (uint64_t)-1;

    uint64_t bars[6];
    for (int i = 0; i < 6; i++)
        bars[i] = d->bars[i];

    PCI::MSIXTable table = kernel.pci.getMSIXTable(d->bus, d->dev, d->func, capOffset, bars);
    if (entry >= table.count)
        return (uint64_t)-1;

    kernel.pci.writeMSIXEntry(table, (uint16_t)entry, (uint8_t)vector);
    kernel.pci.enableMSIX(d->bus, d->dev, d->func, capOffset);
    return 0;
}

static uint64_t syscallGrantCapability(uint64_t targetPid, uint64_t capMask)
{
    Task *caller = kernel.scheduler.current();
    if (!hasCap(caller->capabilities, CAP_GRANT))
        return (uint64_t)-1;
    if (capMask & ~caller->capabilities)
        return (uint64_t)-1; // can't hand out what you don't hold

    Task *target = kernel.scheduler.getTask((uint32_t)targetPid);
    if (!target)
        return (uint64_t)-1;

    target->capabilities |= capMask;
    return 0;
}

static uint64_t syscallRevokeCapability(uint64_t targetPid, uint64_t capMask)
{
    Task *caller = kernel.scheduler.current();
    if (!hasCap(caller->capabilities, CAP_GRANT))
        return (uint64_t)-1;
    if (capMask & ~caller->capabilities)
        return (uint64_t)-1;

    Task *target = kernel.scheduler.getTask((uint32_t)targetPid);
    if (!target)
        return (uint64_t)-1;

    target->capabilities &= ~capMask;
    return 0;
}

// kind: 0=SpecificDevice(param=deviceIndex) 1=ClassWildcard(param=classCode) 2=AnyDevice(param ignored)
static uint64_t syscallGrantDeviceAccess(uint64_t targetPid, uint64_t kind, uint64_t param)
{
    Task *caller = kernel.scheduler.current();
    if (!hasCap(caller->capabilities, CAP_GRANT))
        return (uint64_t)-1;
    if (kind > (uint64_t)DeviceGrantKind::AnyDevice)
        return (uint64_t)-1;

    auto k = (DeviceGrantKind)kind;
    if (!granterCoversGrant(caller, k, param))
        return (uint64_t)-1;

    Task *target = kernel.scheduler.getTask((uint32_t)targetPid);
    if (!target)
        return (uint64_t)-1;

    uint8_t classCode = (k == DeviceGrantKind::ClassWildcard) ? (uint8_t)param : 0;
    uint32_t deviceIdx = (k == DeviceGrantKind::SpecificDevice) ? (uint32_t)param : 0;

    return addDeviceGrant(target, k, classCode, deviceIdx) ? 0 : (uint64_t)-1;
}

static uint64_t syscallGrantShared(uint64_t targetPid, uint64_t handle, uint64_t writeFlag)
{
    if (handle >= s_shared.size() || !s_shared[handle].used)
        return (uint64_t)-1;
    Task *caller = kernel.scheduler.current();
    if (!taskCanMapShared(caller, (uint32_t)handle))
        return (uint64_t)-1;
    Task *target = kernel.scheduler.getTask((uint32_t)targetPid);
    if (!target)
        return (uint64_t)-1;
    return addSharedGrant(target, (uint32_t)handle, writeFlag != 0, caller->id) ? 0 : (uint64_t)-1;
}

static uint64_t syscallRevokeShared(uint64_t targetPid, uint64_t handle)
{
    Task *caller = kernel.scheduler.current();
    Task *target = kernel.scheduler.getTask((uint32_t)targetPid);
    if (!target)
        return (uint64_t)-1;
    for (auto &g : target->sharedGrants)
    {
        if (g.used && g.handle == handle)
        {
            if (s_shared[handle].ownerPid != caller->id && g.grantedBy != caller->id)
                return (uint64_t)-1;
            g.used = false;
            return 0;
        }
    }
    return (uint64_t)-1;
}

extern "C" SyscallResult syscallHandler(SyscallFrame *f)
{
    uint64_t num = f->rdi;
    uint64_t arg1 = f->rsi;
    uint64_t arg2 = f->rdx;
    uint64_t arg3 = f->rcx;
    uint64_t arg4 = f->r8;

    Task *current = kernel.scheduler.current();

    switch (num)
    {
    case SyscallYield:
    {
        SyscallResult r;
        r.rax = 0;
        r.rdx = syscallYield(f);
        return r;
    }
    case SyscallExit:
    {
        SyscallResult r;
        r.rax = 0;
        r.rdx = syscallExit(f);
        return r;
    }
    case SyscallWait:
    {
        SyscallResult r;
        r.rax = 0;
        r.rdx = syscallWait(f);
        return r;
    }
    case SyscallNotify:
    {
        SyscallResult r;
        uint64_t rsp;
        r.rax = syscallNotify(arg1, f, &rsp);
        r.rdx = rsp;
        return r;
    }
    }

    uint64_t ret;
    switch (num)
    {
    case SyscallWrite:
        // if (!hasCap(current->capabilities, CAP_WRITE))
        // {
        //     Serial::printf("[cap] pid=%u DENIED SyscallWrite (missing CAP_WRITE, has=0x%llx)\n",
        //                    current->id, current->capabilities);
        //     Serial::render();
        //     ret = (uint64_t)-1;
        //     break;
        // }
        ret = syscallWrite(arg1, arg2, arg3);
        break;
    case SyscallGetMemoryMap:
        ret = syscallGetMemoryMap(arg1, arg2);
        break;
    case SyscallMmap:
        ret = syscallMmap(arg1, arg2, arg3); 
        break;
    case SyscallMunmap:
        ret = syscallMunmap(arg1, arg2);
        break;
    case SyscallAllocDma:
        ret = syscallAllocDma(arg1, arg2, arg3, arg4);
        break;
    case SyscallCreateShared:
        ret = syscallCreateShared(arg1, arg2, arg3, arg4);
        break;
    case SyscallDestroyShared:
        ret = syscallDestroyShared(arg1);
        break;
    case SyscallMapShared:
        ret = syscallMapShared(arg1, arg2, arg3, arg4);
        break;
    case SyscallUnmapShared:
        ret = syscallUnmapShared(arg1, arg2);
        break;
    case SyscallMapBar:
        ret = syscallMapBar(arg1, arg2, arg3, arg4);  
        break;
    case SyscallUnmapBar:
        ret = syscallUnmapBar(arg1, arg2);
        break;
    case SyscallPciGetDevices:
        ret = syscallPciGetDevices(arg1, arg2);
        break;
    case SyscallPublish:
        ret = syscallPublish(arg1, arg2, arg3, arg4);
        break;
    case SyscallLookup:
        ret = syscallLookup(arg1, arg2, arg3, arg4);
        break;
    case SyscallGetPid:
        ret = syscallGetPid(f);
        break;
    case SyscallSpawn:
        ret = syscallSpawn(arg1);
        break;
    case SyscallGetInfo:
        ret = syscallGetInfo(arg1, arg2, arg3);
        break;
    case SyscallSubscribeIrq:
        if (!hasCap(current->capabilities, CAP_IRQ))
        {
            ret = (uint64_t)-1;
            break;
        }
        ret = syscallSubscribeIrq(arg1);
        break;
    case SyscallAllocVectors:
        if (!hasCap(current->capabilities, CAP_ALLOC_VECTORS))
        {
            ret = (uint64_t)-1;
            break;
        }
        ret = syscallAllocVectors(arg1, arg2);
        break;
    case SyscallPciMsixInfo:
        if (!hasCap(current->capabilities, CAP_PCI_MSIX))
        {
            ret = (uint64_t)-1;
            break;
        }
        ret = syscallPciMsixInfo(arg1, arg2);
        break;
    case SyscallPciMsixEnable:
        if (!hasCap(current->capabilities, CAP_PCI_MSIX))
        {
            ret = (uint64_t)-1;
            break;
        }
        ret = syscallPciMsixEnable(arg1, arg2, arg3);
        break;
    case SyscallGrantCapability:
        ret = syscallGrantCapability(arg1, arg2);
        break;
    case SyscallRevokeCapability:
        ret = syscallRevokeCapability(arg1, arg2);
        break;
    case SyscallGrantDeviceAccess:
        ret = syscallGrantDeviceAccess(arg1, arg2, arg3);
        break;
    default:
        ret = (uint64_t)-1;
        break;
    }

    SyscallResult result;
    result.rax = ret;
    result.rdx = (uint64_t)f;
    return result;
}