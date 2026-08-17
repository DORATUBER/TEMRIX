#pragma once
#include "task.hpp"

class Scheduler {
public:
    void init();

    bool addTask(Task *t);
    uint64_t schedule(uint64_t rsp);
    void sleep(uint32_t ms);
    Task *current();
    void setCurrent(Task *t);
    Task *getTask(uint32_t id);
    uint64_t wakeTask(Task *t, uint64_t rsp);
    uint64_t notifyTask(uint32_t taskId, uint64_t rsp, bool *ok);

    static constexpr int MAX_TASKS = 1024;

private:
    Task *m_tasks[MAX_TASKS] = {};
    int   m_count   = 0;
    uint32_t m_nextId = 1;

    Task *m_current = nullptr;
    Task *m_idle    = nullptr;
};