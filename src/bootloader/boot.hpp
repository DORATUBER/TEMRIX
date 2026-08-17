#pragma once

#include "console.hpp"
#include "input.hpp"
#include "framebuffer.hpp"
#include "fileloader.hpp"
#include "memorymap.hpp"
#include "acpi.hpp"
#include "util.hpp"
#include "memory.hpp"

struct BootContext
{
    Console     con;
    Input       input;
    Framebuffer fb;
    FileLoader  files;
    MemoryMap   mmap;
    void       *rsdp;

    EfiHandle        imageHandle;
    EfiSystemTable  *systemTable;
    EfiBootServices *bs;

    LoadedFile initFile {};

    uint64_t ptPoolAddr  = 0;
    uint64_t ptPoolPages = 320;
    uint64_t stackAddr   = 0;
    uint64_t stackPages  = 4;
    uint64_t kernelAddr  = 0;
    uint64_t kernelSize  = 0;

    uint64_t trampolineAddr = 0;
    static constexpr uint64_t TRAMPOLINE_PHYS = 0x9000;
    static constexpr uint64_t TRAMPOLINE_SIZE = 0x1000;

    BootContext(EfiHandle img, EfiSystemTable *st)
        : rsdp(nullptr), imageHandle(img), systemTable(st), bs(st->BootServices)
    {}

    void init()
    {
        con   = Console(systemTable->ConsoleOut);
        input = Input(systemTable->ConsoleIn);

        con.printfln("Bootloader starting");

        rsdp = findRsdp(systemTable);
        if (rsdp)
            con.printfln("ACPI RSDP found: %llx", (uint64_t)rsdp);
        else
            con.printfln("WARNING: ACPI RSDP not found");

        EfiStatus s = fb.init(bs);
        if (s != EfiSuccess)
        {
            con.printfln("ERROR: GOP/framebuffer init failed");
            halt();
        }

        uint64_t size;
        EfiGraphicsOutputModeInformation *info;
        uint32_t currentMode = fb.gop->Mode->Mode;

        for (uint32_t i = 0; i < fb.gop->Mode->MaxMode; i++)
        {
            if (fb.gop->QueryMode(fb.gop, i, &size, &info) != EfiSuccess)
                continue;

            con.printfln("%d: %dx%d", i, info->HorizontalResolution, info->VerticalResolution);
        }

        con.printfln("Select mode [default %d]: ", currentMode);
        uint32_t chosen = input.readModeIndex(con, currentMode);

        if (chosen >= fb.gop->Mode->MaxMode)
        {
            con.printfln("WARNING: invalid mode %d, keeping current", chosen);
            chosen = currentMode;
        }

        if (chosen != currentMode)
        {
            s = fb.setMode(chosen);
            if (s != EfiSuccess)
            {
                con.printfln("ERROR: Failed to set mode %d", chosen);
                halt();
            }
        }

        con.printfln("Framebuffer base: %llx", fb.base);

        s = files.init(bs, imageHandle);
        if (s != EfiSuccess)
        {
            con.printfln("ERROR: FileSystem init failed");
            halt();
        }
        con.printfln("FileSystem ready");

        s = mmap.acquire(bs);
        if (s != EfiSuccess)
        {
            con.printfln("ERROR: Could not allocate memory map buffer");
            halt();
        }
        con.printfln("Memory map buffer allocated");
    }

    void allocatePageTablePool()
    {
        EfiStatus s = bs->AllocatePages(0, 2, ptPoolPages, &ptPoolAddr);
        if (s != EfiSuccess)
        {
            con.printfln("ERROR: Could not allocate page table pool");
            halt();
        }
        con.printfln("SUCCESS: Allocated page table pool");
    }

    void allocateKernelStack()
    {
        EfiStatus s = bs->AllocatePages(0, 2, stackPages, &stackAddr);
        if (s != EfiSuccess)
        {
            con.printfln("ERROR: Could not allocate kernel stack");
            halt();
        }
        con.printfln("SUCCESS: Allocated kernel stack");
    }

    void loadKernel()
    {
        uint64_t header = 0;
        EfiStatus s = files.readBytes((const uint16_t *)u"\\EFI\\TEMRIX\\kernel.bin", &header, sizeof(header));
        if (s != EfiSuccess)
        {
            con.printfln("ERROR: Failed to read kernel header");
            halt();
        }

        uint64_t realSize = header;
        uint64_t pages = (realSize + 0xFFF) / 0x1000;

        uint64_t fileSize = 0;
        s = files.load((const uint16_t *)u"\\EFI\\TEMRIX\\kernel.bin", pages, kernelAddr, fileSize);
        if (s != EfiSuccess)
        {
            con.printfln("ERROR: Failed to load kernel, status: %llx", (uint64_t)s);
            halt();
        }

        if (realSize < fileSize)
        {
            con.printfln("ERROR: kernel header size smaller than file size");
            halt();
        }

        uint8_t *bssStart = (uint8_t *)(kernelAddr + fileSize);
        for (uint64_t i = 0; i < realSize - fileSize; i++) bssStart[i] = 0;

        kernelSize = realSize;

        con.printfln("Kernel loaded, total size (incl. bss): %llx", kernelSize);
        con.printfln("Kernel physical load address: %llx", kernelAddr);
    }

    void loadUserFile(const uint16_t *name, LoadedFile &out)
    {
        uint64_t fileSize = 0;
        EfiStatus s = files.getSize(name, fileSize);
        if (s != EfiSuccess)
        {
            con.print((const uint16_t *)u"ERROR: Could not get size of: ");
            con.println(name);
            halt();
        }

        uint64_t pages = (fileSize + 0xFFF) / 0x1000;
        uint64_t addr  = 0;
        uint64_t readSize = 0;

        s = files.load(name, pages, addr, readSize);
        if (s != EfiSuccess)
        {
            con.print((const uint16_t *)u"ERROR: Could not load: ");
            con.println(name);
            halt();
        }

        out.physAddr = addr;
        out.size     = readSize;

        con.print((const uint16_t *)u"Loaded ");
        con.println(name);
        con.printfln("  phys=%llx", addr);
        con.printfln("  size=%llx", readSize);
    }

    void allocateTrampoline()
    {
        trampolineAddr = TRAMPOLINE_PHYS;
        EfiStatus s = bs->AllocatePages(1, 2, 1, &trampolineAddr);
        if (s != EfiSuccess)
        {
            con.printfln("WARNING: Could not reserve trampoline at 0x8000");
            trampolineAddr = 0;
        }
        else
        {
            con.printfln("SUCCESS: Reserved trampoline at 0x%llx", trampolineAddr);
        }
    }

    BootInfo exitBootServices()
    {
        EfiStatus s = mmap.exitBootServices(bs, imageHandle);
        if (s != EfiSuccess)
            halt();

        BootInfo info;
        info.Framebuffer       = (uint32_t *)fb.base;
        info.Width             = fb.width;
        info.Height            = fb.height;
        info.PixelsPerScanLine = fb.pitch;
        info.MemoryMap         = mmap.buf;
        info.MemoryMapSize     = mmap.size;
        info.DescriptorSize    = (uint32_t)mmap.descSize;
        info.RSDP              = (void *)rsdp;
        info.trampolineAddr = trampolineAddr;
        return info;
    }

    void setupPageTables(BootInfo& bootInfo)
    {
        const uint64_t stackSize = stackPages * 0x1000;
        const uint64_t stackVirt = 0xFFFFFFFF80000000ULL - stackSize;

        Memory::BumpAllocator bump;
        bump.base   = (uint8_t *)ptPoolAddr;
        bump.offset = 0;
        bump.size   = ptPoolPages * 4096;

        Memory::PageTableContext ptCtx;
        ptCtx.allocator = &bump;

        Memory::initPageTables(
            &ptCtx,
            kernelAddr, kernelSize,
            (uint64_t)bootInfo.Framebuffer, bootInfo.PixelsPerScanLine, bootInfo.Height,
            stackAddr, stackVirt, stackSize,
            (uint8_t *)bootInfo.MemoryMap, bootInfo.MemoryMapSize, bootInfo.DescriptorSize);

        bootInfo.PML4        = (uint64_t)ptCtx.pml4;
        bootInfo.PTPoolBase  = ptPoolAddr;
        bootInfo.PTPoolPages = ptPoolPages;
    }

    [[noreturn]] void jumpToKernel(BootInfo& bootInfo)
    {
        const uint64_t stackSize = stackPages * 0x1000;
        const uint64_t stackVirt = 0xFFFFFFFF80000000ULL - stackSize;

        asm volatile(
            "mov %0, %%rsp\n"
            "mov %1, %%rdi\n"
            "jmp *%2\n"
            :: "r"(stackVirt + stackSize),
               "r"(&bootInfo),
               "r"((uint64_t)0xFFFFFFFF80000000ULL + 8)
            : "memory");

        __builtin_unreachable();
    }
};