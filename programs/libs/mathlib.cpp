#include <stdint.h>

#define TRX_EXPORT extern "C" __attribute__((visibility("default")))

TRX_EXPORT int32_t add(int32_t a, int32_t b) {
    return a + b;
}

TRX_EXPORT int32_t mul(int32_t a, int32_t b) {
    return a * b;
}

TRX_EXPORT uint64_t fib(uint32_t n) {
    uint64_t a = 0, b = 1;
    for (uint32_t i = 0; i < n; i++) {
        uint64_t t = a + b;
        a = b;
        b = t;
    }
    return a;
}