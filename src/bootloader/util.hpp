#pragma once
#include "efi.hpp"

void halt()
{
    while (true) asm volatile("hlt");
}

int32_t memoryCompare(const void *addr1, const void *addr2, uint64_t length)
{
    const uint8_t *a = (const uint8_t *)addr1;
    const uint8_t *b = (const uint8_t *)addr2;
    while (length--)
    {
        if (*a != *b) return (int32_t)*a - (int32_t)*b;
        a++; b++;
    }
    return 0;
}
