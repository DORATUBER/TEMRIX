#include <temrixstd/stdint.h>

static inline bool cpuSupportsRdrand()
{
    uint32_t eax, ebx, ecx, edx;
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    return (ecx & (1u << 30)) != 0; 
}

static inline bool rdrand64(uint64_t *out)
{
    for (int attempts = 0; attempts < 10; attempts++)
    {
        uint8_t ok;
        asm volatile("rdrand %0; setc %1" : "=r"(*out), "=qm"(ok) :: "cc");
        if (ok) return true;
    }
    return false;
}

static inline uint64_t weakFallbackRandom()
{
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    uint64_t x = ((uint64_t)hi << 32) | lo;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return x;
}

static inline uint64_t getRandom64()
{
    static bool checked = false;
    static bool supported = false;
    if (!checked) { supported = cpuSupportsRdrand(); checked = true; }

    uint64_t val;
    if (supported && rdrand64(&val))
        return val;
    return weakFallbackRandom();
}