#pragma once

#include "kernel/common.hpp"
#include "kernel/MemoryCommon.hpp"
#include "kernel/BuddyAllocator.hpp"
#include "kernel/SlabAllocator.hpp"
#include "kernel/PageTable.hpp"
#include "vmm.hpp"
#include "interrupts.hpp"
#include "pci.hpp"
#include "publish.hpp"
#include "scheduler.hpp"

extern uint32_t g_lapic_ticks_per_quantum;

struct Kernel {
    Memory::BuddyAllocator allocator;
    Memory::SlabAllocator slab;
    Memory::PageTableContext ptCtx;
    Memory::VMM vmm;

    Hardware::InterruptController interruptController;
    PCI::Controller pci;

    Scheduler scheduler;
    bool schedulerStarted;

    Publish::Table publishTable;

    uint64_t sharedDataPhysRO;
    uint64_t sharedDataPhysRW;
    KernelReadOnlyData  *sharedDataRO;
    KernelReadWriteData *sharedDataRW;

    FramebufferInfo framebuffer;

    uint64_t rsdpPhys = 0;
};

extern Kernel kernel;