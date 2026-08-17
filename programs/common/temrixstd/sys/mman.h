#pragma once

#include <temrixstd/stdint.h>
#include <temrixstd/stddef.h>
#include <temrixstd/stdio.h>
#include <temrixstd/temrix.h>
#include "pci.h"
#include "info.h"

#ifndef SYSCALL_MEM_DEBUG
#define SYSCALL_MEM_DEBUG 0
#endif

#if SYSCALL_MEM_DEBUG
#define MEM_DBG(fmt, ...) String::Printf("[mem] %s <- %s: " fmt "\n", __func__, caller, ##__VA_ARGS__)
#else
#define MEM_DBG(fmt, ...) do {} while (0)
#endif

namespace Memory
{
static constexpr uint64_t Read = 1ULL << 0;
static constexpr uint64_t Write = 1ULL << 1;
static constexpr uint64_t Execute = 1ULL << 2;
static constexpr uint64_t User = 1ULL << 3;
static constexpr uint64_t NoCache = 1ULL << 4;
static constexpr uint64_t Huge = 1ULL << 5;
static constexpr uint64_t Discontig = 1ULL << 6;
static constexpr uint64_t Mmio = 1ULL << 7;

    static inline void Copy(void *dest, const void *src, size_t n)
    {
        uint64_t *d64 = (uint64_t *)dest;
        const uint64_t *s64 = (const uint64_t *)src;
        size_t chunks = n / 8;
        for (size_t i = 0; i < chunks; i++)
            d64[i] = s64[i];

        uint8_t *d8 = (uint8_t *)(d64 + chunks);
        const uint8_t *s8 = (const uint8_t *)(s64 + chunks);
        for (size_t i = 0; i < n % 8; i++)
            d8[i] = s8[i];
    }

    static inline void Set(void *dest, uint8_t value, size_t count)
    {
        uint8_t *ptr = (uint8_t *)dest;
        for (size_t i = 0; i < count; i++)
            ptr[i] = value;
    }

    static inline int Compare(const void *a, const void *b, size_t count)
    {
        const uint8_t *pa = (const uint8_t *)a;
        const uint8_t *pb = (const uint8_t *)b;
        for (size_t i = 0; i < count; i++)
        {
            if (pa[i] != pb[i])
                return (int)pa[i] - (int)pb[i];
        }
        return 0;
    }
}

static inline void *memcpy(void *dest, const void *src, size_t n)
{
    Memory::Copy(dest, src, n);
    return dest;
}

static inline void *memset(void *dest, int value, size_t n)
{
    Memory::Set(dest, (uint8_t)value, n);
    return dest;
}

static inline int memcmp(const void *a, const void *b, size_t n)
{
    return Memory::Compare(a, b, n);
}

namespace Syscall
{
namespace Memory
{
    struct DmaAllocResult { uint64_t virt, phys, size; };
    struct SharedMemResult { uint64_t handle, virtAddr, pageCount; };
    struct VMA { uint64_t start, end, flags; };

    static inline uint64_t GetMemoryMap(VMA *buf, uint64_t maxEntries)
    {
        return syscall2(SyscallGetMemoryMap, (uint64_t)buf, maxEntries);
    }

    namespace detail
    {
        inline VMA s_vmas[512];
        inline uint64_t s_count = 0;
        inline uint64_t s_cursor = 0x0000000000400000ULL;

        inline void Bootstrap()
        {
            uint64_t total = GetMemoryMap(nullptr, 0);
            uint64_t toFetch = total < 512 ? total : 512;
            s_count = GetMemoryMap(s_vmas, toFetch);
            if (s_count > 512) s_count = 512;
        }

        inline uint64_t FindGap(uint64_t size)
        {
            uint64_t c = s_cursor;
        restart:
            for (uint64_t i = 0; i < s_count; i++)
            {
                if (c < s_vmas[i].end && c + size > s_vmas[i].start)
                {
                    c = s_vmas[i].end;
                    goto restart;
                }
            }
            return c;
        }

        inline void Track(uint64_t start, uint64_t size)
        {
            if (s_count < 512)
                s_vmas[s_count++] = { start, start + size, 0 };
            s_cursor = start + size;
        }

        inline void Untrack(uint64_t start, const char *caller)
        {
            for (uint64_t i = 0; i < s_count; i++)
            {
                if (s_vmas[i].start == start)
                {
                    s_vmas[i] = s_vmas[--s_count];
                    return;
                }
            }
            MEM_DBG("addr=%p not found in tracked vmas", start);
        }
    }

    static inline void Init() { detail::Bootstrap(); }

    static inline uint64_t Map(uint64_t size,
                               uint64_t flags = ::Memory::Read | ::Memory::Write | ::Memory::User,
                               const char *caller = __builtin_FUNCTION())
    {
        uint64_t aligned = (size + 0xFFF) & ~0xFFFULL;
        for (int attempt = 0; attempt < 4; attempt++)
        {
            uint64_t addr = detail::FindGap(aligned);
            uint64_t ret = syscall3(SyscallMmap, addr, aligned, flags);
            if (ret != 0)
            {
                MEM_DBG("addr=%p size=%p flags=%p", ret, aligned, flags);
                detail::Track(addr, aligned);
                return ret;
            }
            detail::Bootstrap();
        }
        MEM_DBG("FAILED size=%p flags=%p", aligned, flags);
        return 0;
    }

    static inline void Unmap(uint64_t ptr, uint64_t size,
                              const char *caller = __builtin_FUNCTION())
    {
        MEM_DBG("addr=%p size=%p", ptr, size);
        syscall2(SyscallMunmap, ptr, size);
        detail::Untrack(ptr, caller);
    }

    static inline uint64_t AllocDma(uint64_t size, uint64_t flags, DmaAllocResult *out,
                                     const char *caller = __builtin_FUNCTION())
    {
        uint64_t aligned = (size + 0xFFF) & ~0xFFFULL;
        for (int attempt = 0; attempt < 4; attempt++)
        {
            uint64_t addr = detail::FindGap(aligned);
            uint64_t ret = syscall4(SyscallAllocDma, addr, aligned, flags, (uint64_t)out);
            if (ret == 0)
            {
                MEM_DBG("virt=%p phys=%p size=%p", out ? out->virt : 0, out ? out->phys : 0, aligned);
                detail::Track(addr, aligned);
                return 0;
            }
            detail::Bootstrap();
        }
        MEM_DBG("FAILED size=%p flags=%p", aligned, flags);
        return (uint64_t)-1;
    }

    static inline uint64_t CreateShared(uint64_t pageCount, uint64_t flags, SharedMemResult *out,
                                         const char *caller = __builtin_FUNCTION())
    {
        uint64_t size = pageCount * 0x1000;
        for (int attempt = 0; attempt < 4; attempt++)
        {
            uint64_t addr = detail::FindGap(size);
            uint64_t ret = syscall4(SyscallCreateShared, addr, pageCount, flags, (uint64_t)out);
            if (ret == 0)
            {
                MEM_DBG("handle=%llu virtAddr=%p pageCount=%llu",
                    out ? out->handle : 0, out ? out->virtAddr : 0, out ? out->pageCount : 0);
                detail::Track(addr, size);
                return 0;
            }
            detail::Bootstrap();
        }
        MEM_DBG("FAILED pageCount=%llu flags=%p", pageCount, flags);
        return (uint64_t)-1;
    }

    static inline uint64_t DestroyShared(uint64_t handle,
                                          const char *caller = __builtin_FUNCTION())
    {
        MEM_DBG("handle=%llu", handle);
        return syscall1(SyscallDestroyShared, handle);
    }

    static inline uint64_t MapShared(uint64_t handle, uint64_t flags, SharedMemResult *out,
                                      const char *caller = __builtin_FUNCTION())
    {
        uint32_t pageCount = 0;
        if (syscall3(SyscallGetInfo, InfoSharedRegion, handle, (uint64_t)&pageCount) != 0)
        {
            MEM_DBG("FAILED to query pageCount for handle=%llu", handle);
            return (uint64_t)-1;
        }

        uint64_t size = (uint64_t)pageCount * 0x1000;
        for (int attempt = 0; attempt < 4; attempt++)
        {
            uint64_t addr = detail::FindGap(size);
            uint64_t ret = syscall4(SyscallMapShared, addr, handle, flags, (uint64_t)out);
            if (ret == 0)
            {
                MEM_DBG("virtAddr=%p handle=%llu pageCount=%llu",
                    out ? out->virtAddr : 0, out ? out->handle : 0, out ? out->pageCount : 0);
                detail::Track(addr, size);
                return 0;
            }
            detail::Bootstrap();
        }
        MEM_DBG("FAILED handle=%llu", handle);
        return (uint64_t)-1;
    }

    static inline void UnmapShared(uint64_t virtAddr, uint64_t pageCount,
                                    const char *caller = __builtin_FUNCTION())
    {
        MEM_DBG("virtAddr=%p pageCount=%llu", virtAddr, pageCount);
        syscall2(SyscallUnmapShared, virtAddr, pageCount);
        detail::Untrack(virtAddr, caller);
    }

    static inline uint64_t MapBar(uint64_t deviceIndex, uint64_t barIndex,
                                uint64_t flags = ::Memory::Read | ::Memory::Write | ::Memory::User | ::Memory::NoCache,
                                const char *caller = __builtin_FUNCTION())
    {
        Syscall::Pci::KernelDevice dev;
        if (syscall3(SyscallGetInfo, InfoPciDevice, deviceIndex, (uint64_t)&dev) != 0)
        {
            MEM_DBG("FAILED to query device info for deviceIndex=%llu", deviceIndex);
            return 0;
        }

        uint64_t size = dev.barSizes[barIndex];
        if (size == 0)
            return 0;

        for (int attempt = 0; attempt < 4; attempt++)
        {
            uint64_t addr = detail::FindGap(size);
            uint64_t ret = syscall4(SyscallMapBar, addr, deviceIndex, barIndex, flags);
            if (ret != 0)
            {
                MEM_DBG("addr=%p deviceIndex=%llu barIndex=%llu size=%p", ret, deviceIndex, barIndex, size);
                detail::Track(addr, size);
                return ret;
            }
            detail::Bootstrap();
        }
        MEM_DBG("FAILED deviceIndex=%llu barIndex=%llu", deviceIndex, barIndex);
        return 0;
    }

    static inline void UnmapBar(uint64_t virtAddr, uint64_t size,
                                 const char *caller = __builtin_FUNCTION())
    {
        MEM_DBG("virtAddr=%p size=%p", virtAddr, size);
        syscall2(SyscallUnmapBar, virtAddr, size);
        detail::Untrack(virtAddr, caller);
    }
}
}