#include "string.hpp"

namespace String {
    int equal(const char* s1, const char* s2) {
        int i = 0;
        while (s1[i] != '\0' && s2[i] != '\0') {
            if (s1[i] != s2[i]) return 0;
            i++;
        }
        return (s1[i] == '\0' && s2[i] == '\0');
    }

    int len(const char* s) {
        int n = 0;
        while (s[n]) n++;
        return n;
    }

    int compare(const char *a, const char *b, uint32_t n)
    {
        for (uint32_t i = 0; i < n; i++)
        {
            if (a[i] != b[i])
                return 1;
            if (!a[i])
                break;
        }
        return 0;
    }

    void intToStr(int val, char* buf) {
        if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
        char tmp[20];
        int i = 0;
        bool neg = val < 0;
        if (neg) val = -val;
        while (val > 0) {
            int q = val / 10;
            tmp[i++] = '0' + (val - q * 10);
            val = q;
        }
        int j = 0;
        if (neg) buf[j++] = '-';
        while (i > 0) buf[j++] = tmp[--i];
        buf[j] = '\0';
    }

    void uintToStr(uint32_t val, char* buf) {
        if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
        char tmp[20];
        int i = 0;
        while (val > 0) {
            uint32_t q = val / 10;
            tmp[i++] = '0' + (val - q * 10);
            val = q;
        }
        int j = 0;
        while (i > 0) buf[j++] = tmp[--i];
        buf[j] = '\0';
    }

    void uintToHexStr(uint32_t val, char* buf) {
        if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
        char tmp[8];
        int i = 0;
        while (val > 0) {
            uint8_t nibble = val & 0xF;
            tmp[i++] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
            val >>= 4;
        }
        int j = 0;
        while (i > 0) buf[j++] = tmp[--i];
        buf[j] = '\0';
    }

    void uint64ToHexStr(uint64_t val, char* buf) {
        if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
        char tmp[16];
        int i = 0;
        while (val > 0) {
            uint8_t nibble = val & 0xF;
            tmp[i++] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
            val >>= 4;
        }
        int j = 0;
        while (i > 0) buf[j++] = tmp[--i];
        buf[j] = '\0';
    }

    void kvsnprintf(char* buf, int size, const char* fmt, __builtin_va_list args) {
        int bi = 0;
        for (int i = 0; fmt[i] && bi < size - 1; i++) {
            if (fmt[i] != '%') { buf[bi++] = fmt[i]; continue; }
            i++;

            bool left_align = false;
            char pad_char   = ' ';
            if (fmt[i] == '-') { left_align = true; i++; }
            if (fmt[i] == '0' && !left_align) { pad_char = '0'; i++; }

            int pad_width = 0;
            while (fmt[i] >= '0' && fmt[i] <= '9') {
                pad_width = pad_width * 10 + (fmt[i] - '0');
                i++;
            }

            bool is_long_long = false;
            if (fmt[i] == 'l' && fmt[i+1] == 'l') {
                is_long_long = true;
                i += 2;
            }

            char tmp[32];
            tmp[0] = '\0';

            switch (fmt[i]) {
                case 'd':
                    intToStr(__builtin_va_arg(args, int), tmp);
                    break;
                case 'u':
                    if (is_long_long)
                        uintToStr((uint32_t)__builtin_va_arg(args, uint64_t), tmp);
                    else
                        uintToStr(__builtin_va_arg(args, uint32_t), tmp);
                    break;
                case 'x':
                    if (is_long_long)
                        uint64ToHexStr(__builtin_va_arg(args, uint64_t), tmp);
                    else
                        uintToHexStr(__builtin_va_arg(args, uint32_t), tmp);
                    break;
                case 'p': {
                    uint64_t addr = __builtin_va_arg(args, uint64_t);
                    if (bi < size - 1) buf[bi++] = '0';
                    if (bi < size - 1) buf[bi++] = 'x';
                    for (int shift = 60; shift >= 0 && bi < size - 1; shift -= 4) {
                        uint8_t nibble = (addr >> shift) & 0xF;
                        buf[bi++] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
                    }
                    continue;
                }
                case 's': {
                    const char* s = __builtin_va_arg(args, const char*);
                    if (!s) s = "(null)";
                    int slen = 0;
                    while (s[slen]) slen++;
                    if (!left_align)
                        while (pad_width > slen && bi < size - 1) { buf[bi++] = pad_char; pad_width--; }
                    while (*s && bi < size - 1) buf[bi++] = *s++;
                    if (left_align)
                        while (pad_width > slen && bi < size - 1) { buf[bi++] = ' '; pad_width--; }
                    continue;
                }
                case 'c':
                    buf[bi++] = (char)__builtin_va_arg(args, int);
                    continue;
                case '%':
                    buf[bi++] = '%';
                    continue;
                default:
                    buf[bi++] = '%';
                    buf[bi++] = fmt[i];
                    continue;
            }

            int tmp_len = 0;
            while (tmp[tmp_len]) tmp_len++;
            if (!left_align)
                while (pad_width > tmp_len && bi < size - 1) { buf[bi++] = pad_char; pad_width--; }
            for (int j = 0; tmp[j] && bi < size - 1; j++)
                buf[bi++] = tmp[j];
            if (left_align)
                while (pad_width > tmp_len && bi < size - 1) { buf[bi++] = ' '; pad_width--; }
        }
        buf[bi] = '\0';
    }

    void ksprintf(char* buf, int size, const char* fmt, ...) {
        __builtin_va_list args;
        __builtin_va_start(args, fmt);
        kvsnprintf(buf, size, fmt, args);
        __builtin_va_end(args);
    }
}