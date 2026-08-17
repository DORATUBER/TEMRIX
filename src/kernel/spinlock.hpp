#pragma once

struct Spinlock {
    volatile int locked = 0;

    void acquire() {
        while (true) {
            if (!__atomic_load_n(&locked, __ATOMIC_RELAXED)) {
                if (!__atomic_test_and_set(&locked, __ATOMIC_ACQUIRE))
                    return;
            }
            asm volatile("pause");
        }
    }

    void release() {
        __atomic_clear(&locked, __ATOMIC_RELEASE);
    }
};