#include "common.hpp"
#include "MemoryCommon.hpp"
#include "BuddyAllocator.hpp"
#include "PageTable.hpp"
#include "vmm.hpp"
#include "arch/x86_64/gdt.hpp"
#include "arch/x86_64/tss.hpp"
#include "arch/x86_64/cpu.hpp"
#include "KernelState.hpp"
#include "interrupts.hpp"
#include "timer.hpp"
#include "Serial.hpp"
#include "loader/trx.hpp"
#include "graphics.hpp"
#include "syscall.hpp"
#include "SyscallWrappers.hpp"
#include "arch/x86_64/simd.hpp"

Kernel kernel;

Graphics::FrameBuffer frontframebuffer;

void *operator new(size_t size) { return kernel.allocator.malloc(size); }
void *operator new[](size_t size) { return kernel.allocator.malloc(size); }
void operator delete(void *ptr) { kernel.allocator.free(ptr); }
void operator delete(void *ptr, size_t) { kernel.allocator.free(ptr); }
void operator delete[](void *ptr) { kernel.allocator.free(ptr); }
void operator delete[](void *ptr, size_t) { kernel.allocator.free(ptr); }

static const char* pciClassName(uint8_t classCode)
{
    switch (classCode)
    {
        case 0x00: return "Unclassified";
        case 0x01: return "Mass Storage";
        case 0x02: return "Network";
        case 0x03: return "Display";
        case 0x04: return "Multimedia";
        case 0x05: return "Memory";
        case 0x06: return "Bridge";
        case 0x07: return "Comm Ctrl";
        case 0x08: return "Sys Periph";
        case 0x09: return "Input Dev";
        case 0x0A: return "Dock Station";
        case 0x0B: return "Processor";
        case 0x0C: return "Serial Bus";
        case 0x0D: return "Wireless";
        case 0x0E: return "Intel IO";
        case 0x0F: return "Satellite";
        case 0x10: return "Encryption";
        case 0x11: return "Signal Proc";
        default:   return "Unknown";
    }
}

static void printPCITable(PCI::Controller& pci)
{
    uint32_t count = pci.getDeviceCount();

    Serial::printf("[pci] enumerated %u device(s)\n", count);
    Serial::printf("+------+-----+-----+-----+--------+--------+-------+-------+-----+------------+------------------+\n");
    Serial::printf("| Idx  | Bus | Dev | Fn  | VenID  | DevID  | Class | SubCl | Rev | BAR0       | Class Name       |\n");
    Serial::printf("+------+-----+-----+-----+--------+--------+-------+-------+-----+------------+------------------+\n");

    for (uint32_t i = 0; i < count; i++)
    {
        const PCI::KernelDevice* d = pci.getDevice(i);
        if (!d || !d->valid) continue;

        Serial::printf("| %-4u | %-3u | %-3u | %-3u | 0x%04x | 0x%04x | 0x%02x  | 0x%02x  | %-3u | 0x%08x | %-16s |\n",
            i,
            d->bus,
            d->dev,
            d->func,
            d->vendorId,
            d->deviceId,
            d->classCode,
            d->subclass,
            d->revision,
            (uint32_t)d->bars[0],
            pciClassName(d->classCode));
    }

    Serial::printf("+------+-----+-----+-----+--------+--------+-------+-------+-----+------------+------------------+\n");

    Serial::printf("\n[pci] BAR details:\n");
    for (uint32_t i = 0; i < count; i++)
    {
        const PCI::KernelDevice* d = pci.getDevice(i);
        if (!d || !d->valid) continue;

        Serial::printf("  Device %u (bus %u dev %u func %u):\n", i, d->bus, d->dev, d->func);
        for (int b = 0; b < 6; b++)
        {
            if (d->bars[b] == 0) continue;
            Serial::printf("    BAR%d = 0x%016llx  size = 0x%llx\n", b, d->bars[b], d->barSizes[b]);
        }
    }
}

__attribute__((section(".text._start"))) void _start(BootInfo *info)
{
    Hardware::GDT::init();
    Hardware::TSS::init();

    kernel.allocator.init(info);
    kernel.slab.init(&kernel.allocator);

    kernel.framebuffer.physAddr = (uint64_t)info->Framebuffer;
    kernel.framebuffer.width = info->Width;
    kernel.framebuffer.height = info->Height;
    kernel.framebuffer.pixelsPerScanLine = info->PixelsPerScanLine;

    frontframebuffer.data = (uint32_t *)Memory::phys_to_virt((uint64_t)info->Framebuffer);
    frontframebuffer.Width = info->Width;
    frontframebuffer.Height = info->Height;
    frontframebuffer.PixelsPerScanLine = info->PixelsPerScanLine;

    Serial::init(frontframebuffer);
    Serial::print("[boot] Kernel starting\n");
    Serial::render();

    Hardware::Simd::Init();

    Serial::printf("Boot framebuffer: %ux%u\n",
                   frontframebuffer.Width,
                   frontframebuffer.Height);

    kernel.ptCtx.pml4 = (uint64_t *)Memory::phys_to_virt(info->PML4);
    kernel.ptCtx.alloc = &kernel.allocator;

    LoadedFile init = info->initProcess;
    uint8_t *initVirtAddress = (uint8_t *)Memory::phys_to_virt(init.physAddr);
    uint64_t initSize = init.size;

    for (int i = 0; i < 256; i++)
        kernel.ptCtx.pml4[i] = 0;

    Memory::mapPage4KB(&kernel.ptCtx,
                       Memory::phys_to_virt(0xFEE00000),
                       0xFEE00000,
                       Memory::PAGE_WRITABLE | Memory::PAGE_PCD | Memory::PAGE_PWT);

    kernel.vmm.init(&kernel.allocator, &kernel.ptCtx, Memory::KERNEL_VMM_BASE);
    kernel.scheduler.init();

    void *roPage = kernel.allocator.malloc(0x1000);
    void *rwPage = kernel.allocator.malloc(0x1000);
    Memory::set(roPage, 0, 0x1000);
    Memory::set(rwPage, 0, 0x1000);

    kernel.sharedDataPhysRO = Memory::virt_to_phys((uint64_t)roPage);
    kernel.sharedDataPhysRW = Memory::virt_to_phys((uint64_t)rwPage);

    Memory::mapPage4KB(&kernel.ptCtx, KERNEL_RO_DATA_ADDRESS, kernel.sharedDataPhysRO, Memory::PAGE_WRITABLE);
    Memory::mapPage4KB(&kernel.ptCtx, KERNEL_RW_DATA_ADDRESS, kernel.sharedDataPhysRW, Memory::PAGE_WRITABLE);

    kernel.sharedDataRO = (KernelReadOnlyData *)KERNEL_RO_DATA_ADDRESS;
    kernel.sharedDataRW = (KernelReadWriteData *)KERNEL_RW_DATA_ADDRESS;
    kernel.sharedDataRO->ticks = 0;
    kernel.sharedDataRO->ticksPerSecond = 100;
    kernel.sharedDataRO->kbHead = 0;
    kernel.sharedDataRW->kbTail = 0;

    kernel.interruptController.init();
    kernel.interruptController.registerExceptions();
    Hardware::IDT::setEntry(Hardware::Vector::LAPIC_TIMER, (void *)Hardware::ISR::apTimerStub);
    Hardware::IDT::setEntry(Hardware::Vector::SYSCALL, (void *)Hardware::ISR::syscallStub, Hardware::IDT::USER_INTERRUPT);

    Hardware::PIT::init(100);
    kernel.interruptController.registerMasterIRQ(Hardware::Vector::TIMER, Hardware::MasterIRQ::TIMER, (void *)Hardware::ISR::timer);
    kernel.interruptController.registerMasterIRQ(Hardware::Vector::KEYBOARD,
                                                 Hardware::MasterIRQ::KEYBOARD,
                                                 (void *)Hardware::ISR::keyboard);
    Hardware::initLapicTimer();

    Serial::render();

    kernel.pci.enumerateAll();
    printPCITable(kernel.pci);

    kernel.publishTable.init(&kernel.allocator);

    Serial::print("[boot] scheduler started, entering idle\n");

    Task* initTask = nullptr;
    if (TRX::load(initVirtAddress, initSize, &initTask)){
        Serial::print("[trx] Succeded to load init process\n");

        initTask->capabilities = CAP_ALL_SIMPLE;
        initTask->deviceGrants[0] = { true, DeviceGrantKind::AnyDevice, 0, 0 };

        kernel.scheduler.addTask(initTask);
        Serial::printf("[boot] init module spawned pid=%d\n", initTask->id);
    }

    Serial::render();

    kernel.schedulerStarted = true;

    while (true){
        asm volatile("hlt");
    }
}