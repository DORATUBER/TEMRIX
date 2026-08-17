#pragma once

typedef unsigned char uint8_t;
typedef char int8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned long uint64_t;
typedef long int64_t;
typedef unsigned long uintptr_t;

typedef uint64_t size_t;
typedef int64_t ssize_t;

#if defined(__SIZEOF_INT128__)
    typedef unsigned __int128 uint128_t;
    typedef __int128 int128_t;
    #define HAS_UINT128 1
#else
    #define HAS_UINT128 0
#endif

struct LoadedFile {
    uint64_t physAddr;
    uint64_t size;
};

struct BootInfo {
    uint32_t* Framebuffer;
    uint32_t Width;
    uint32_t Height;
    uint32_t PixelsPerScanLine;
    void* MemoryMap;
    uint32_t MemoryMapSize;
    uint32_t DescriptorSize;
    void* RSDP;
    uint64_t PML4;
    uint64_t PTPoolBase;
    uint64_t PTPoolPages;
    uint64_t trampolineAddr;
    LoadedFile initProcess;
};

static constexpr uint64_t KERNEL_RO_DATA_ADDRESS = 0x3FE000; 
static constexpr uint64_t KERNEL_RW_DATA_ADDRESS = 0x3FF000; 

struct KernelReadOnlyData {
    volatile uint64_t ticks;
    volatile uint64_t ticksPerSecond;
    volatile uint8_t  kbBuf[256];
    volatile uint8_t  kbHead;
    uint8_t _pad[4096 - (8 + 8 + 256 + 1)];
};
static_assert(sizeof(KernelReadOnlyData) == 0x1000);

struct KernelReadWriteData {
    volatile uint8_t kbTail;
    uint8_t _pad[4096 - 1];
};
static_assert(sizeof(KernelReadWriteData) == 0x1000);

struct FramebufferInfo
{
    uint64_t physAddr;
    uint32_t width;
    uint32_t height;
    uint32_t pixelsPerScanLine;
};

#define UNUSED(x) (void)(x)
#define offsetof(type, member) __builtin_offsetof(type, member)