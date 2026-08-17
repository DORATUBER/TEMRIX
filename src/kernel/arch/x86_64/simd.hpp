#pragma once
#include "common.hpp"
#include "Serial.hpp"

namespace Hardware::Simd
{
    inline uint32_t g_xsaveAreaSize = 512;
    inline bool g_hasAvx = false;
    inline uint32_t g_xsaveMaskLo = 0x7;
    inline uint32_t g_xsaveMaskHi = 0;

    inline void Init()
    {
        uint32_t eax, ebx, ecx, edx;
        asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1), "c"(0));
        g_hasAvx = (ecx & (1u << 28)) != 0;
        bool hasXsave = (ecx & (1u << 26)) != 0;
        bool hasOsXsave = (ecx & (1u << 27)) != 0;

        Serial::printf("[simd] cpuid.1: AVX=%u XSAVE=%u OSXSAVE=%u\n",
                       g_hasAvx, hasXsave, hasOsXsave);

        if (!g_hasAvx)
        {
            Serial::printf("[simd] AVX not supported by CPU, staying on legacy fxsave/fxrstor (%u bytes)\n",
                           g_xsaveAreaSize);
            return;
        }

        if (!hasXsave)
        {
            Serial::printf("[simd] AVX reported but XSAVE not supported (unexpected), falling back to %u bytes\n",
                           g_xsaveAreaSize);
            return;
        }

        if (!hasOsXsave)
        {
            Serial::printf("[simd] WARNING: XSAVE supported but OSXSAVE not set, did enableSse() run "
                           "before this, and did CR4.OSXSAVE actually get set? Falling back to %u bytes\n",
                           g_xsaveAreaSize);
            return;
        }

        uint32_t xcr0Lo, xcr0Hi;
        asm volatile("xgetbv" : "=a"(xcr0Lo), "=d"(xcr0Hi) : "c"(0));
        bool xcr0X87 = (xcr0Lo & (1u << 0)) != 0;
        bool xcr0Sse = (xcr0Lo & (1u << 1)) != 0;
        bool xcr0Avx = (xcr0Lo & (1u << 2)) != 0;

        Serial::printf("[simd] XCR0 readback: raw=0x%08x x87=%u sse=%u avx=%u\n",
                       xcr0Lo, xcr0X87, xcr0Sse, xcr0Avx);

        if (!xcr0Avx)
        {
            Serial::printf("[simd] WARNING: AVX bit not set in XCR0 despite OSXSAVE being on, "
                           "enableSse()'s xsetbv likely didn't run or didn't include XCR0_AVX. "
                           "Falling back to %u bytes\n",
                           g_xsaveAreaSize);
            return;
        }

        asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0xD), "c"(0));
        g_xsaveAreaSize = ebx;

        Serial::printf("[simd] AVX enabled OK, xsave area size = %u bytes "
                       "(cpuid.0xD.0: max_size=%u, xcr0_supported_mask=0x%08x)\n",
                       g_xsaveAreaSize, ecx, eax);
    }

    inline void Save(uint8_t *area)
    {
        if (g_hasAvx)
            asm volatile("xsave %0" : "=m"(*area) : "a"(g_xsaveMaskLo), "d"(g_xsaveMaskHi) : "memory");
        else
            asm volatile("fxsave %0" :: "m"(*area) : "memory");
    }

    inline void Restore(uint8_t *area)
    {
        if (g_hasAvx)
            asm volatile("xrstor %0" :: "m"(*area), "a"(g_xsaveMaskLo), "d"(g_xsaveMaskHi) : "memory");
        else
            asm volatile("fxrstor %0" :: "m"(*area) : "memory");
    }

    inline uint32_t AreaSize()
    {
        return g_hasAvx ? g_xsaveAreaSize : 512;
    }

    inline uint32_t AreaAlignment()
    {
        return g_hasAvx ? 64 : 16;
    }
}