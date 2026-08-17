#include "interrupts.hpp"
#include "timer.hpp"
#include "KernelState.hpp"
#include "Serial.hpp"

extern Kernel kernel;

extern "C" uint64_t apTimerHandler(uint64_t rsp)
{
    volatile uint32_t *lapic = (volatile uint32_t *)Memory::phys_to_virt(0xFEE00000);
    lapic[0xB0 / 4] = 0; 

    Hardware::rearmLapicTimerIfNeeded(); 

    Hardware::g_ticks+=1;
    if (kernel.sharedDataPhysRO)
    {
        auto *roKernel = (KernelReadOnlyData *)Memory::phys_to_virt(kernel.sharedDataPhysRO);
        roKernel->ticks = Hardware::g_ticks;
    }
    
    if (!kernel.schedulerStarted)
        return rsp;

    uint64_t new_rsp = kernel.scheduler.schedule(rsp);
    Task *current = kernel.scheduler.current();
    if (current && current->pageTable)
        asm volatile("mov %0, %%cr3" ::"r"(
            Memory::virt_to_phys((uint64_t)current->pageTable->pml4)) : "memory");

    return new_rsp;
}

struct PageFaultResult
{
    uint64_t retval;
    uint64_t newRsp;
};

extern "C" PageFaultResult pagefault_handler(uint64_t error_code, uint64_t rsp)
{
    uint64_t faulty_address;
    asm volatile("mov %%cr2, %0" : "=r"(faulty_address));

    Task *dead_task = kernel.schedulerStarted ? kernel.scheduler.current() : nullptr;

    if (dead_task)
    {
        Serial::printf("\n[KERNEL] Task %d crashed at %p! Error Code: %d\n", dead_task->id, (void *)faulty_address, (uint32_t)error_code);

        
        uint64_t *stack = (uint64_t *)rsp;
        for (int i = 14; i >= 0; i--)
            stack[i + 1] = stack[i];

        dead_task->rsp = rsp + 8;
        dead_task->state = Task::State::PendingDelete;

        uint64_t new_rsp = kernel.scheduler.schedule(dead_task->rsp);

        Task *next = kernel.scheduler.current();
        if (next && next->pageTable)
            asm volatile("mov %0, %%cr3" ::"r"(Memory::virt_to_phys((uint64_t)next->pageTable->pml4)) : "memory");

        return {0, new_rsp};
    }

    Serial::printf("\n[KERNEL PANIC] Page fault at %p error=%d rsp=%p\n", (void *)faulty_address, (uint32_t)error_code, (void *)rsp);

    asm volatile("cli; hlt");
    __builtin_unreachable();
}

static const char *exceptionName(uint64_t vector)
{
    switch (vector)
    {
    case 0:
        return "Divide Error";
    case 1:
        return "Debug";
    case 3:
        return "Breakpoint";
    case 4:
        return "Overflow";
    case 5:
        return "Bound Range Exceeded";
    case 6:
        return "Invalid Opcode";
    case 7:
        return "Device Not Available";
    case 10:
        return "Invalid TSS";
    case 11:
        return "Segment Not Present";
    case 12:
        return "Stack Fault";
    case 13:
        return "General Protection Fault";
    case 16:
        return "x87 FPU Error";
    case 17:
        return "Alignment Check";
    case 19:
        return "SIMD Exception";
    default:
        return "Unknown Exception";
    }
}

static void printBacktrace(uint64_t rbp, int maxFrames = 20)
{
    Serial::printf("[trace] backtrace:\n");
    for (int i = 0; i < maxFrames; i++)
    {
        if (rbp == 0 || (rbp & 0x7))
            break;

        uint64_t *frame = (uint64_t *)rbp;
        uint64_t savedRbp = frame[0];
        uint64_t retAddr  = frame[1];

        if (retAddr == 0)
            break;

        Serial::printf("[trace]   #%d rbp=0x%llx ret=0x%llx\n", i, rbp, retAddr);

        if (savedRbp <= rbp)
            break;
        rbp = savedRbp;
    }
    Serial::render();
}

extern "C" uint64_t exceptionHandlerNamed(uint64_t vector, uint64_t rsp)
{
    uint64_t rip = ((uint64_t *)rsp)[15];
    uint64_t savedRbp = ((uint64_t *)rsp)[4];

    Task *dead = kernel.schedulerStarted ? kernel.scheduler.current() : nullptr;
    if (dead)
    {
        Serial::printf("\n[KERNEL] Task %d exception %s (vector %u) at RIP=0x%llx\n", dead->id, exceptionName(vector), (uint32_t)vector, rip);
        printBacktrace(savedRbp);  
        dead->state = Task::State::PendingDelete;
        uint64_t new_rsp = kernel.scheduler.schedule(rsp);
        Task *next = kernel.scheduler.current();
        if (next && next->pageTable)
            asm volatile("mov %0, %%cr3" ::"r"(Memory::virt_to_phys((uint64_t)next->pageTable->pml4)) : "memory");
        return new_rsp;
    }

    Serial::printf("\n[KERNEL PANIC] %s (vector %u) at RIP=0x%llx\n",
                      exceptionName(vector), (uint32_t)vector, rip);

    asm volatile("cli; hlt");
    __builtin_unreachable();
}

extern "C" uint64_t exceptionErrorHandlerNamed(uint64_t vector, uint64_t rsp, uint64_t code)
{
    uint64_t rip = ((uint64_t *)rsp)[16];
    uint64_t savedRbp = ((uint64_t *)rsp)[4]; 

    Task *dead = kernel.schedulerStarted ? kernel.scheduler.current() : nullptr;
    if (dead)
    {
        Serial::printf("\n[KERNEL] Task %d exception %s (vector %u) at RIP=0x%llx err=0x%llx\n", dead->id, exceptionName(vector), (uint32_t)vector, rip, code);
        printBacktrace(savedRbp); 

        uint64_t *stack = (uint64_t *)rsp;
        for (int i = 14; i >= 0; i--)
            stack[i + 1] = stack[i];

        dead->rsp = rsp + 8;
        dead->state = Task::State::PendingDelete;
        uint64_t new_rsp = kernel.scheduler.schedule(dead->rsp);
        Task *next = kernel.scheduler.current();
        if (next && next->pageTable)
            asm volatile("mov %0, %%cr3" ::"r"(Memory::virt_to_phys((uint64_t)next->pageTable->pml4)) : "memory");
        return new_rsp;
    }

    Serial::printf("\n[KERNEL PANIC] %s (vector %u) at RIP=0x%llx err=0x%llx\n",
                      exceptionName(vector), (uint32_t)vector, rip, code);

    asm volatile("cli; hlt");
    __builtin_unreachable();
}

extern "C" uint64_t msixHandler(uint8_t vector, uint64_t rsp)
{
    volatile uint32_t *lapic = (volatile uint32_t *)Memory::phys_to_virt(0xFEE00000);
    lapic[0xB0 / 4] = 0;

    Serial::printf("[msix] fired vector=%u\n", vector);   

    return Hardware::irqDispatch(vector, rsp);
}

extern "C" uint64_t (*msixStubTable[206])();

namespace Hardware
{
    IrqWaiter g_irqWaiters[MAX_IRQ_WAITERS] = {};

    uint64_t irqDispatch(uint8_t vector, uint64_t rsp)
    {
        for (auto &w : g_irqWaiters)
        {
            if (!w.used || !w.enabled || w.vector != vector) continue;
            w.enabled = false;
            if (!w.task) continue;

            if (w.task->state == Task::State::Waiting)
                w.task->state = Task::State::Ready;   
            else
                w.task->state = Task::State::Notified;
            break;
        }
        return rsp;   
    }
}

namespace Hardware
{
    void InterruptController::init()
    {
        asm volatile("cli");

        GDT::init();
        IDT::init();
        PIC::init();
        TSS::init();
        IDT::load();

        LAPIC::init();
        IDT::setEntry(Vector::LAPIC_SPURIOUS,
                      (void *)Hardware::ISR::lapicSpuriousISR);

        PS2::drainBuffer();

        asm volatile("sti");

        auto *r1 = (VectorRange *)kernel.slab.malloc(sizeof(VectorRange));
        r1->base = Vector::DYNAMIC_BASE;
        r1->count = Vector::SYSCALL - Vector::DYNAMIC_BASE;
        r1->next = nullptr;

        auto *r2 = (VectorRange *)kernel.slab.malloc(sizeof(VectorRange));
        r2->base = Vector::SYSCALL + 1;
        r2->count = Vector::LAPIC_SPURIOUS - Vector::SYSCALL - 1;
        r2->next = nullptr;

        r1->next = r2;
        m_freeList = r1;

        for (uint32_t v = Vector::DYNAMIC_BASE; v <= Vector::DYNAMIC_MAX; v++)
        {
            if (v == Vector::SYSCALL)
                continue;
            registerMSIX((uint8_t)v, (void *)msixStubTable[v - Vector::DYNAMIC_BASE]);
        }
    }

    void InterruptController::registerExceptions()
    {
        registerHandler(Vector::DIVIDE_ERROR, (void *)exceptionStub0);
        registerHandler(Vector::DEBUG, (void *)exceptionStub1);
        registerHandler(Vector::BREAKPOINT, (void *)exceptionStub3);
        registerHandler(Vector::OVERFLOW, (void *)exceptionStub4);
        registerHandler(Vector::BOUND_RANGE, (void *)exceptionStub5);
        registerHandler(Vector::INVALID_OPCODE, (void *)exceptionStub6);
        registerHandler(Vector::DEVICE_NOT_AVAIL, (void *)exceptionStub7);
        registerHandler(Vector::FPU_ERROR, (void *)exceptionStub16);
        registerHandler(Vector::SIMD_ERROR, (void *)exceptionStub19);

        registerHandler(Vector::INVALID_TSS, (void *)exceptionErrorStub10);
        registerHandler(Vector::SEGMENT_NOT_PRESENT, (void *)exceptionErrorStub11);
        registerHandler(Vector::STACK_FAULT, (void *)exceptionErrorStub12);
        registerHandler(Vector::GENERAL_PROTECTION, (void *)exceptionErrorStub13);
        registerHandler(Vector::ALIGNMENT_CHECK, (void *)exceptionErrorStub17);

        registerHandler(Vector::PAGE_FAULT, (void *)pagefault_isr);

        IDT::setEntry(Vector::NMI, (void *)ISR::nmi, Hardware::IDT::KERNEL_INTERRUPT, TSS::IST_NMI);
        IDT::setEntry(Vector::DOUBLE_FAULT, (void *)ISR::doubleFault, Hardware::IDT::KERNEL_INTERRUPT, TSS::IST_CRITICAL);
        IDT::setEntry(Vector::MACHINE_CHECK, (void *)ISR::machineCheck, Hardware::IDT::KERNEL_INTERRUPT, TSS::IST_CRITICAL);
    }

    VectorAllocation Hardware::InterruptController::allocVectors(uint32_t count)
    {
        VectorAllocation result = {};

        for (VectorRange *r = m_freeList; r; r = r->next)
        {
            if (r->count >= count)
            {
                
                result.ranges[0] = {r->base, (uint8_t)count, nullptr};
                result.rangeCount = 1;
                r->base += (uint8_t)count;
                r->count -= (uint8_t)count;
                return result;
            }
            
            if (result.rangeCount == 0)
            {
                result.ranges[0] = {r->base, r->count, nullptr};
                result.rangeCount = 1;
                count -= r->count;
                r->count = 0;
                continue;
            }
            
            uint8_t take = (uint8_t)count < r->count ? (uint8_t)count : r->count;
            result.ranges[1] = {r->base, take, nullptr};
            result.rangeCount = 2;
            r->base += take;
            r->count -= take;
            count -= take;
            if (count == 0)
                return result;
        }
        
        return {};
    }

    void Hardware::InterruptController::freeVectors(VectorAllocation alloc)
    {
        for (uint8_t i = 0; i < alloc.rangeCount; i++)
        {
            auto *node = (VectorRange *)kernel.allocator.malloc(sizeof(VectorRange));
            node->base = alloc.ranges[i].base;
            node->count = alloc.ranges[i].count;
            node->next = m_freeList;
            m_freeList = node;
        }
        mergeAdjacent();
    }

    void Hardware::InterruptController::mergeAdjacent()
    {
        
        for (VectorRange *a = m_freeList; a; a = a->next)
            for (VectorRange *b = a->next; b; b = b->next)
                if (b->base < a->base)
                {
                    uint8_t tb = a->base;
                    a->base = b->base;
                    b->base = tb;
                    uint8_t tc = a->count;
                    a->count = b->count;
                    b->count = tc;
                }

        
        for (VectorRange *r = m_freeList; r && r->next;)
        {
            if (r->base + r->count == r->next->base)
            {
                r->count += r->next->count;
                VectorRange *old = r->next;
                r->next = old->next;
                kernel.slab.free(old);
            }
            else
            {
                r = r->next;
            }
        }
    }
}

namespace Hardware {

    static inline void cpuid(uint32_t leaf, uint32_t subleaf,
                              uint32_t &eax, uint32_t &ebx,
                              uint32_t &ecx, uint32_t &edx) {
        asm volatile("cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(leaf), "c"(subleaf));
    }

    static inline uint64_t rdtsc() {
        uint32_t lo, hi;
        asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
        return ((uint64_t)hi << 32) | lo;
    }

    static inline void wrmsr(uint32_t msr, uint64_t value) {
        uint32_t lo = (uint32_t)(value & 0xFFFFFFFF);
        uint32_t hi = (uint32_t)(value >> 32);
        asm volatile("wrmsr" :: "a"(lo), "d"(hi), "c"(msr));
    }

    static bool s_tscDeadlineSupported = false;
    static uint64_t s_tscHz = 0;

    static bool checkTscDeadline() {
        uint32_t eax, ebx, ecx, edx;
        cpuid(1, 0, eax, ebx, ecx, edx);
        return (ecx & (1u << 24)) != 0;
    }

    static uint64_t tscFrequencyFromCpuid() {
        uint32_t eax, ebx, ecx, edx;
        cpuid(0, 0, eax, ebx, ecx, edx);
        if (eax >= 0x15) {
            cpuid(0x15, 0, eax, ebx, ecx, edx);
            if (eax != 0 && ebx != 0 && ecx != 0)
                return (uint64_t)ecx * ebx / eax;
        }
        return 0;
    }

    static uint64_t measureTscFrequency() {
        constexpr uint64_t SAMPLE_TICKS = 50;

        uint64_t sync = g_ticks;
        while (g_ticks == sync)
            asm volatile("pause");

        uint64_t tick_start = g_ticks;
        uint64_t tsc_start = rdtsc();

        while (g_ticks < tick_start + SAMPLE_TICKS)
            asm volatile("pause");

        uint64_t tsc_end = rdtsc();
        uint64_t elapsed_ticks = g_ticks - tick_start;

        uint64_t cycles = tsc_end - tsc_start;
        return (cycles / elapsed_ticks) * 100; 
    }

    void initLapicTimer() {
        volatile uint32_t *lapic = (volatile uint32_t *)Memory::phys_to_virt(0xFEE00000);

        lapic[0xF0 / 4] = 0x1FF; 

        s_tscDeadlineSupported = checkTscDeadline();

        if (s_tscDeadlineSupported) {
            bool fromCpuid = true;
            s_tscHz = tscFrequencyFromCpuid();
            if (s_tscHz == 0) {
                fromCpuid = false;
                s_tscHz = measureTscFrequency();
            }

            lapic[0x320 / 4] = (0x2 << 17) | Vector::LAPIC_TIMER; 

            PIC::maskMaster(MasterIRQ::TIMER);

            Serial::printf("[lapic] using TSC-deadline, tsc_hz=%llu (source=%s)\n",
                           s_tscHz, fromCpuid ? "cpuid" : "measured");

            wrmsr(0x6E0 /* IA32_TSC_DEADLINE */, rdtsc() + s_tscHz / 100);
            return;
        }

        lapic[0x3E0 / 4] = 0xB;
        lapic[0x320 / 4] = 0x00010000; 
        lapic[0x380 / 4] = 0xFFFFFFFF;

        constexpr uint64_t SAMPLE_TICKS = 50;

        uint64_t sync = g_ticks;
        while (g_ticks == sync)
            asm volatile("pause");

        uint64_t tick_start = g_ticks;
        uint32_t lapic_start = lapic[0x390 / 4];

        while (g_ticks < tick_start + SAMPLE_TICKS)
            asm volatile("pause");

        uint32_t lapic_end = lapic[0x390 / 4];
        uint64_t elapsed_ticks = g_ticks - tick_start;

        PIC::maskMaster(MasterIRQ::TIMER);

        uint32_t ticks_per_10ms = (uint32_t)((lapic_start - lapic_end) / elapsed_ticks);

        lapic[0x3E0 / 4] = 0xB;
        lapic[0x320 / 4] = (0x1 << 17) | Vector::LAPIC_TIMER;
        lapic[0x380 / 4] = ticks_per_10ms;

        Serial::printf("[lapic] no TSC-deadline, calibrated periodic, ticks_per_10ms=%u\n", ticks_per_10ms);
    }

    void rearmLapicTimerIfNeeded() {
        if (s_tscDeadlineSupported)
            wrmsr(0x6E0, rdtsc() + s_tscHz / 100);
    }

}