#pragma once
#include "common.hpp"
#include "capability.hpp"
#include "SharedGrant.hpp"
#include "kernel/MemoryCommon.hpp"
#include "kernel/BuddyAllocator.hpp"
#include "kernel/PageTable.hpp"
#include "vmm.hpp"
#include "arch/x86_64/gdt.hpp"

struct InitFrame {
    uint64_t r15, r14, r13, r12, rbp, rbx, r11, r10, r9, r8, rdi, rsi, rdx, rcx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
};

struct InitialRegisters {
    uint64_t rdi = 0, rsi = 0, rdx = 0, rcx = 0, r8 = 0, r9 = 0;
    uint64_t rbx = 0, rbp = 0, r12 = 0, r13 = 0, r14 = 0, r15 = 0;
};

class Task {
public:
    uint64_t rsp;
    uint64_t *stack;
    size_t stackSize;
    uint64_t *kernelStack; 
    uint32_t id;
    uint64_t sleepUntil = 0;   
    uint64_t virtualTime = 0;  
    uint64_t schedStart = 0;   
    Memory::PageTableContext* pageTable = nullptr; 
    Memory::VMM* userVmm;  

    uint8_t *fpuenv = nullptr; 

    enum class State : uint8_t {
        Ready, Running, Sleeping, Waiting, Notified, Dead, PendingDelete
    } state;

    bool notifyPending = false; 

    uint64_t    capabilities = CAP_NONE;
    DeviceGrant deviceGrants[MAX_DEVICE_GRANTS];
    SharedGrant sharedGrants[MAX_SHARED_GRANTS];

    static Task *createKernelShell();
    static bool   finalizeKernel(Task *t, uint64_t entry, uint64_t stackPointer);

    static Task *createUserShell(Memory::PageTableContext *pageTable);
    static bool   finalizeUser(Task *t, uint64_t entry, uint64_t stackPointer,
                            const InitialRegisters &regs = {});
    void destroy();
};