#include "task.hpp"
#include "KernelState.hpp"
#include "Serial.hpp"
#include "arch/x86_64/simd.hpp"

extern Kernel kernel;

static uint8_t *defaultFpuEnv = nullptr;
static bool defaultFpuEnvReady = false;

static uint8_t *allocFpuArea()
{
    uint32_t size  = Hardware::Simd::AreaSize();

    uint8_t *addr = (uint8_t *)kernel.allocator.malloc(size);
    return (uint8_t *)addr;
}

static void ensureDefaultFpuEnv() {
    if (defaultFpuEnvReady) return;

    defaultFpuEnv = allocFpuArea();
    Memory::set(defaultFpuEnv, 0, Hardware::Simd::AreaSize());

    asm volatile("fninit");
    Hardware::Simd::Save(defaultFpuEnv);

    
    
    *(uint32_t*)(defaultFpuEnv + 24) = 0x1F80;

    defaultFpuEnvReady = true;
}

static Task *allocTaskBase(Memory::PageTableContext *pageTable, size_t stackSize)
{
    Task *t = new Task();
    if (!t)
        return nullptr;

    t->kernelStack = (uint64_t *)kernel.vmm.allocDiscontiguous(16 * 1024, Memory::VM_READ | Memory::VM_WRITE);
    if (!t->kernelStack)
    {
        delete t;
        return nullptr;
    }

    t->fpuenv = allocFpuArea();
    if (!t->fpuenv)
    {
        kernel.vmm.free(t->kernelStack, 16 * 1024);
        delete t;
        return nullptr;
    }

    t->pageTable = pageTable;
    t->state = Task::State::Ready;
    t->id = 0;
    t->userVmm = nullptr;
    t->stack = nullptr;
    t->stackSize = stackSize;
    t->capabilities = CAP_NONE;
    for (auto &g : t->deviceGrants)
        g.used = false;
    return t;
}

static bool mapAddressSpace(Task *t, Memory::PageTableContext *pageTable, uint64_t roFlags)
{
    t->userVmm = new Memory::VMM();
    if (!t->userVmm)
        return false;
    t->userVmm->init(&kernel.allocator, pageTable);

    for (int i = 256; i < 512; i++)
        pageTable->pml4[i] = kernel.ptCtx.pml4[i];

    Memory::mapPage4KB(pageTable, KERNEL_RO_DATA_ADDRESS, kernel.sharedDataPhysRO, roFlags);
    Memory::mapPage4KB(pageTable, KERNEL_RW_DATA_ADDRESS, kernel.sharedDataPhysRW, roFlags | Memory::PAGE_WRITABLE);

    t->userVmm->track(KERNEL_RO_DATA_ADDRESS, KERNEL_RO_DATA_ADDRESS + 0x1000, Memory::VM_MMIO);
    t->userVmm->track(KERNEL_RW_DATA_ADDRESS, KERNEL_RW_DATA_ADDRESS + 0x1000, Memory::VM_MMIO);
    return true;
}

static InitFrame *initFrameSkeleton(Task *t)
{
    uint8_t *kernelStackTop = (uint8_t *)t->kernelStack + 16 * 1024;
    kernelStackTop -= sizeof(InitFrame);
    auto *f = (InitFrame *)kernelStackTop;
    Memory::set(f, 0, sizeof(InitFrame));
    ensureDefaultFpuEnv();
    Memory::copy(t->fpuenv, defaultFpuEnv, Hardware::Simd::AreaSize());
    t->rsp = (uint64_t)kernelStackTop;
    return f;
}

Task *Task::createKernelShell()
{
    return allocTaskBase(nullptr, 0);
}

bool Task::finalizeKernel(Task *t, uint64_t entry, uint64_t stackPointer)
{
    InitFrame *f = initFrameSkeleton(t);
    f->rip = entry;
    f->cs = Hardware::GDT::KERNEL_CODE;
    f->rflags = 0x202;
    f->rsp = stackPointer;
    f->ss = Hardware::GDT::KERNEL_DATA;
    return true;
}

Task *Task::createUserShell(Memory::PageTableContext *pageTable)
{
    Task *t = allocTaskBase(pageTable, 0);
    if (!t)
        return nullptr;
    if (!mapAddressSpace(t, pageTable, Memory::PAGE_USER))
    {
        delete t;
        return nullptr;
    }
    return t;
}

bool Task::finalizeUser(Task *t, uint64_t entry, uint64_t stackPointer, const InitialRegisters &regs)
{
    InitFrame *f = initFrameSkeleton(t);
    f->rip = entry;
    f->cs = Hardware::GDT::USER_CODE;
    f->rflags = 0x202;
    f->rsp = stackPointer;
    f->ss = Hardware::GDT::USER_DATA;
    f->rdi = regs.rdi;
    f->rsi = regs.rsi;
    f->rdx = regs.rdx;
    f->rcx = regs.rcx;
    f->r8 = regs.r8;
    f->r9 = regs.r9;
    f->rbx = regs.rbx;
    f->rbp = regs.rbp;
    f->r12 = regs.r12;
    f->r13 = regs.r13;
    f->r14 = regs.r14;
    f->r15 = regs.r15;
    return true;
}

void Task::destroy()
{
    if (userVmm)
    {
        userVmm->destroyAll();
        delete userVmm;
        userVmm = nullptr;
    }

    if (pageTable)
    {
        Memory::freePageTable(pageTable, &kernel.allocator);
        delete pageTable;
        pageTable = nullptr;
    }

    if (stack)
    {
        kernel.vmm.free(stack, stackSize);
        stack = nullptr;
    }

    if (kernelStack)
    {
        kernel.vmm.free(kernelStack, 16 * 1024);
        kernelStack = nullptr;
    }

    if (fpuenv)
    {
        kernel.slab.free(fpuenv);
        fpuenv = nullptr;
    }

    delete this;
}