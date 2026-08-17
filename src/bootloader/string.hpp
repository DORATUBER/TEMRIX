#pragma once
#include "efi.hpp"

namespace String {
    int  equal(const char* s1, const char* s2);
    int  len(const char* s);
    int compare(const char *a, const char *b, uint32_t n);
    void intToStr(int val, char* buf);
    void uintToHexStr(uint32_t val, char* buf);
    void kvsnprintf(char* buf, int size, const char* fmt, __builtin_va_list args);
    void ksprintf(char* buf, int size, const char* fmt, ...);
}