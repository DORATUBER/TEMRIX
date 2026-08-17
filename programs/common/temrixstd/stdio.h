#pragma once

#include <temrixstd/stdint.h>
#include <temrixstd/stddef.h>
#include <temrixstd/stdbool.h>
#include <temrixstd/temrix.h>
#include <temrixstd/string.h>

namespace Syscall
{
    namespace IO
    {
        static inline uint64_t Write(const void *buf, uint64_t len)
        {
            return syscall3(SyscallWrite, (uint64_t)buf, len, 0);
        }

        static inline uint64_t Write(const char *str)
        {
            uint64_t len = 0;
            while (str[len])
                len++;
            return syscall3(SyscallWrite, (uint64_t)str, len, 0);
        }

        static inline uint64_t Flush()
        {
            return syscall3(SyscallWrite, 0, 0, 1 << 0);
        }
    }
}

namespace String
{
    static inline void vsnprintf(char *buf, int size, const char *fmt, __builtin_va_list args)
    {
        int bi = 0;
        for (int i = 0; fmt[i] && bi < size - 1; i++)
        {
            if (fmt[i] != '%')
            {
                buf[bi++] = fmt[i];
                continue;
            }
            i++;

            bool left_align = false;
            char pad_char = ' ';
            if (fmt[i] == '-')
            {
                left_align = true;
                i++;
            }
            if (fmt[i] == '0' && !left_align)
            {
                pad_char = '0';
                i++;
            }

            int pad_width = 0;
            while (fmt[i] >= '0' && fmt[i] <= '9')
            {
                pad_width = pad_width * 10 + (fmt[i] - '0');
                i++;
            }

            bool is_long_long = false;
            if (fmt[i] == 'l' && fmt[i + 1] == 'l')
            {
                is_long_long = true;
                i += 2;
            }

            char tmp[32];
            tmp[0] = '\0';

            switch (fmt[i])
            {
            case 'd':
                IntToStr(__builtin_va_arg(args, int), tmp);
                break;
            case 'u':
                if (is_long_long)
                    Uint64ToStr(__builtin_va_arg(args, uint64_t), tmp);
                else
                    UintToStr(__builtin_va_arg(args, uint32_t), tmp);
                break;
            case 'x':
                if (is_long_long)
                    Uint64ToHexStr(__builtin_va_arg(args, uint64_t), tmp);
                else
                    UintToHexStr(__builtin_va_arg(args, uint32_t), tmp);
                break;
            case 'p':
            {
                uint64_t addr = __builtin_va_arg(args, uint64_t);
                if (bi < size - 1)
                    buf[bi++] = '0';
                if (bi < size - 1)
                    buf[bi++] = 'x';
                for (int shift = 60; shift >= 0 && bi < size - 1; shift -= 4)
                {
                    uint8_t n = (addr >> shift) & 0xF;
                    buf[bi++] = n < 10 ? '0' + n : 'a' + n - 10;
                }
                continue;
            }
            case 's':
            {
                const char *s = __builtin_va_arg(args, const char *);
                if (!s)
                    s = "(null)";
                int slen = 0;
                while (s[slen])
                    slen++;
                if (!left_align)
                    while (pad_width > slen && bi < size - 1)
                    {
                        buf[bi++] = pad_char;
                        pad_width--;
                    }
                while (*s && bi < size - 1)
                    buf[bi++] = *s++;
                if (left_align)
                    while (pad_width > slen && bi < size - 1)
                    {
                        buf[bi++] = ' ';
                        pad_width--;
                    }
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
            while (tmp[tmp_len])
                tmp_len++;
            if (!left_align)
                while (pad_width > tmp_len && bi < size - 1)
                {
                    buf[bi++] = pad_char;
                    pad_width--;
                }
            for (int j = 0; tmp[j] && bi < size - 1; j++)
                buf[bi++] = tmp[j];
            if (left_align)
                while (pad_width > tmp_len && bi < size - 1)
                {
                    buf[bi++] = ' ';
                    pad_width--;
                }
        }
        buf[bi] = '\0';
    }

    static inline void snprintf(char *buf, int size, const char *fmt, ...)
    {
        __builtin_va_list args;
        __builtin_va_start(args, fmt);
        vsnprintf(buf, size, fmt, args);
        __builtin_va_end(args);
    }

    static inline void Printf(const char *fmt, ...)
    {
        char buf[4096];

        __builtin_va_list args;
        __builtin_va_start(args, fmt);

        String::vsnprintf(buf, sizeof(buf), fmt, args);

        __builtin_va_end(args);

        Syscall::IO::Write(buf);
    }

    static inline void Print(const char *str)
    {
        Syscall::IO::Write(str, Length(str));
    }
}
