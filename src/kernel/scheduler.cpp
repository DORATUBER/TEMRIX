#include "scheduler.hpp"
#include "KernelState.hpp"
#include "timer.hpp"
#include "arch/x86_64/tss.hpp"
#include "Serial.hpp"
#include "arch/x86_64/simd.hpp"

static void idleTask() {
    while (true)
        asm volatile("sti; hlt");
}

void Scheduler::init() {
    m_count  = 0;
    m_nextId = 1;

    for (int i = 0; i < MAX_TASKS; i++)
        m_tasks[i] = nullptr;

    m_current = nullptr;

    constexpr uint64_t IDLE_STACK_SIZE = 16 * 1024;
    void *idleStack = kernel.vmm.allocDiscontiguous(IDLE_STACK_SIZE, Memory::VM_READ | Memory::VM_WRITE);
    if (!idleStack) {
        Serial::print("[sched] fatal: idle stack allocation failed\n");
        Serial::render();
        asm volatile("cli; hlt");
    }

    m_idle = Task::createKernelShell();
    if (!m_idle) {
        Serial::print("[sched] fatal: idle task shell allocation failed\n");
        Serial::render();
        asm volatile("cli; hlt");
    }

    if (!Task::finalizeKernel(m_idle, (uint64_t)idleTask, (uint64_t)idleStack + IDLE_STACK_SIZE)) {
        Serial::print("[sched] fatal: idle task finalize failed\n");
        Serial::render();
        asm volatile("cli; hlt");
    }
}

bool Scheduler::addTask(Task *t) {
    if (!t) return false;
    if (m_count >= MAX_TASKS) return false;

    t->id = m_nextId++;
    m_tasks[m_count++] = t;
    return true;
}

void Scheduler::sleep(uint32_t ms) {
    m_current->sleepUntil = Hardware::ticks() + ms;
    m_current->state = Task::State::Sleeping;
}

Task *Scheduler::current() {
    return m_current;
}

void Scheduler::setCurrent(Task *t) {
    m_current = t;
}

Task *Scheduler::getTask(uint32_t id) {
    for (int i = 0; i < m_count; i++) {
        if (m_tasks[i]->id == id) return m_tasks[i];
    }
    return nullptr;
}

uint64_t Scheduler::schedule(uint64_t rsp) {
    for (int i = 0; i < m_count; i++) {
        if (m_tasks[i]->state == Task::State::Dead) {
            Task *target = m_tasks[i];
            for (int j = i; j < m_count - 1; j++)
                m_tasks[j] = m_tasks[j + 1];
            m_count--;
            target->destroy();
            i--;
        }
    }

    for (int i = 0; i < m_count; i++) {
        if (m_tasks[i]->state == Task::State::Sleeping &&
            Hardware::ticks() >= m_tasks[i]->sleepUntil)
            m_tasks[i]->state = Task::State::Ready;
    }

    if (m_current &&
        m_current->state != Task::State::PendingDelete &&
        m_current->state != Task::State::Dead) {
        m_current->rsp = rsp;
        m_current->virtualTime += Hardware::ticks() - m_current->schedStart;
    }

    Task *old_task = m_current;

    Task *next = nullptr;
    for (int i = 0; i < m_count; i++) {
        Task *t = m_tasks[i];
        if (t->state == Task::State::Ready ||
            t->state == Task::State::Running ||
            t->state == Task::State::Notified) {
            if (!next || t->virtualTime < next->virtualTime)
                next = t;
        }
    }

    if (!next) {
        m_current = m_idle;
    } else {
        m_current = next;
    }

    if (old_task && old_task != next)
        Hardware::Simd::Save(old_task->fpuenv);

    m_current->state = Task::State::Running;
    m_current->schedStart = Hardware::ticks();
    Hardware::TSS::setRsp0((uint64_t)m_current->kernelStack + 16 * 1024);

    if (m_current != old_task)
        Hardware::Simd::Restore(m_current->fpuenv);

    if (old_task && old_task->state == Task::State::PendingDelete)
        old_task->state = Task::State::Dead;

    return m_current->rsp;
}

uint64_t Scheduler::wakeTask(Task *t, uint64_t rsp) {
    if (m_current &&
        m_current->state != Task::State::PendingDelete &&
        m_current->state != Task::State::Dead) {
        m_current->rsp = rsp;
        m_current->virtualTime += Hardware::ticks() - m_current->schedStart;
    }

    Task *old_task = m_current;
    if (old_task && old_task != t)
        Hardware::Simd::Save(old_task->fpuenv);

    t->state      = Task::State::Running;
    t->schedStart = Hardware::ticks();
    m_current     = t;

    if (t != old_task)
        Serial::printf("[sched] switch: %u -> %u (reason=notify)\n",
                    old_task ? old_task->id : 0, t->id);

    Hardware::TSS::setRsp0((uint64_t)t->kernelStack + 16 * 1024);

    if (t != old_task)
        Hardware::Simd::Restore(t->fpuenv);

    return t->rsp;
}

uint64_t Scheduler::notifyTask(uint32_t taskId, uint64_t rsp, bool *ok) {
    Task *t = nullptr;
    for (int i = 0; i < m_count; i++)
        if (m_tasks[i]->id == taskId) { t = m_tasks[i]; break; }

    if (!t || t == m_current || t->state == Task::State::Dead) {
        *ok = false;
        return rsp;
    }

    *ok = true;

    if (t->state == Task::State::Waiting)
        return wakeTask(t, rsp); 

    t->notifyPending = true;
    return rsp;
}