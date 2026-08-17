#include "MemoryCommon.hpp"

namespace Memory {
    void set(void* dest, uint8_t value, size_t count) {
        uint8_t* ptr = (uint8_t*)dest;
        for (size_t i = 0; i < count; i++)
            ptr[i] = value;
    }

    void copy(void* dest, const void* src, size_t count) {
        uint8_t* d       = (uint8_t*)dest;
        const uint8_t* s = (const uint8_t*)src;
        for (size_t i = 0; i < count; i++)
            d[i] = s[i];
    }

    int compare(const void* a, const void* b, size_t count) {
        const uint8_t* pa = (const uint8_t*)a;
        const uint8_t* pb = (const uint8_t*)b;
        for (size_t i = 0; i < count; i++)
        {
            if (pa[i] != pb[i])
                return (int)pa[i] - (int)pb[i];
        }
        return 0;
    }

    extern "C" void* memset(void* ptr, int value, size_t size) {
        uint8_t* p = (uint8_t*)ptr;
        while (size--) *p++ = (uint8_t)value;
        return ptr;
    }
}