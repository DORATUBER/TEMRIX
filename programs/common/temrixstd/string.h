#pragma once

#include <temrixstd/stdint.h>
#include <temrixstd/stddef.h>

namespace String
{
    static inline size_t Length(const char *s)
    {
        size_t n = 0;
        while (s[n])
            n++;
        return n;
    }

    static inline int Compare(const char *a, const char *b)
    {
        while (*a && (*a == *b))
        {
            a++;
            b++;
        }
        return (int)(unsigned char)*a - (int)(unsigned char)*b;
    }

    static inline int CompareN(const char *a, const char *b, size_t n)
    {
        for (size_t i = 0; i < n; i++)
        {
            unsigned char ca = (unsigned char)a[i];
            unsigned char cb = (unsigned char)b[i];
            if (ca != cb)
                return (int)ca - (int)cb;
            if (ca == '\0')
                return 0;
        }
        return 0;
    }

    static inline const char *FindChar(const char *s, int c)
    {
        while (*s)
        {
            if (*s == (char)c)
                return s;
            s++;
        }
        return (c == '\0') ? s : nullptr;
    }

    static inline int Equal(const char *a, const char *b)
    {
        int i = 0;
        while (a[i] && b[i])
        {
            if (a[i] != b[i])
                return 0;
            i++;
        }
        return a[i] == '\0' && b[i] == '\0';
    }

    static inline int Compare(const char *a, const char *b, uint32_t n)
    {
        for (uint32_t i = 0; i < n; i++)
        {
            if (a[i] != b[i])
                return a[i] - b[i];
            if (!a[i])
                break;
        }
        return 0;
    }

    static inline char *FromU64(uint64_t val, char *buf)
    {
        buf[20] = '\0';
        int i = 19;
        if (val == 0)
        {
            buf[i--] = '0';
        }
        else
        {
            while (val)
            {
                buf[i--] = '0' + (val % 10);
                val /= 10;
            }
        }
        return &buf[i + 1];
    }

    static inline void IntToStr(int32_t val, char *buf)
    {
        if (val == 0)
        {
            buf[0] = '0';
            buf[1] = '\0';
            return;
        }
        char tmp[20];
        int i = 0;
        bool neg = val < 0;
        if (neg)
            val = -val;
        while (val > 0)
        {
            int q = val / 10;
            tmp[i++] = '0' + (val - q * 10);
            val = q;
        }
        int j = 0;
        if (neg)
            buf[j++] = '-';
        while (i > 0)
            buf[j++] = tmp[--i];
        buf[j] = '\0';
    }

    static inline void UintToStr(uint32_t val, char *buf)
    {
        if (val == 0)
        {
            buf[0] = '0';
            buf[1] = '\0';
            return;
        }
        char tmp[20];
        int i = 0;
        while (val > 0)
        {
            uint32_t q = val / 10;
            tmp[i++] = '0' + (val - q * 10);
            val = q;
        }
        int j = 0;
        while (i > 0)
            buf[j++] = tmp[--i];
        buf[j] = '\0';
    }

    static inline void Uint64ToStr(uint64_t val, char *buf)
    {
        if (val == 0)
        {
            buf[0] = '0';
            buf[1] = '\0';
            return;
        }
        char tmp[20];
        int i = 0;
        while (val > 0)
        {
            uint64_t q = val / 10;
            tmp[i++] = '0' + (val - q * 10);
            val = q;
        }
        int j = 0;
        while (i > 0)
            buf[j++] = tmp[--i];
        buf[j] = '\0';
    }

    static inline void UintToHexStr(uint32_t val, char *buf)
    {
        if (val == 0)
        {
            buf[0] = '0';
            buf[1] = '\0';
            return;
        }
        char tmp[8];
        int i = 0;
        while (val > 0)
        {
            uint8_t n = val & 0xF;
            tmp[i++] = n < 10 ? '0' + n : 'a' + n - 10;
            val >>= 4;
        }
        int j = 0;
        while (i > 0)
            buf[j++] = tmp[--i];
        buf[j] = '\0';
    }

    static inline void Uint64ToHexStr(uint64_t val, char *buf)
    {
        if (val == 0)
        {
            buf[0] = '0';
            buf[1] = '\0';
            return;
        }
        char tmp[16];
        int i = 0;
        while (val > 0)
        {
            uint8_t n = val & 0xF;
            tmp[i++] = n < 10 ? '0' + n : 'a' + n - 10;
            val >>= 4;
        }
        int j = 0;
        while (i > 0)
            buf[j++] = tmp[--i];
        buf[j] = '\0';
    }

    static inline bool IsSpace(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
    }

    static inline int DigitValue(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'z') return c - 'a' + 10;
        if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
        return -1;
    }

    static inline long StrToL(const char *str, char **endptr, int base)
    {
        const char *p = str;
        while (IsSpace(*p)) p++;

        bool neg = false;
        if (*p == '+' || *p == '-')
        {
            neg = (*p == '-');
            p++;
        }

        if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
        {
            p += 2;
            base = 16;
        }
        else if (base == 0)
        {
            base = (p[0] == '0') ? 8 : 10;
        }

        static const long kLongMax = 0x7FFFFFFFFFFFFFFFLL;
        static const long kLongMin = -kLongMax - 1;

        long result = 0;
        bool any = false;
        bool overflow = false;

        for (;;)
        {
            int d = DigitValue(*p);
            if (d < 0 || d >= base)
                break;

            any = true;
            if (!overflow)
            {
                if (result > (kLongMax - d) / base)
                    overflow = true;
                else
                    result = result * base + d;
            }
            p++;
        }

        if (endptr)
            *endptr = (char *)(any ? p : str);

        if (overflow)
            return neg ? kLongMin : kLongMax;

        return neg ? -result : result;
    }
}

static inline size_t strlen(const char *s) { return String::Length(s); }
static inline int strcmp(const char *a, const char *b) { return String::Compare(a, b); }
static inline int strncmp(const char *a, const char *b, size_t n) { return String::CompareN(a, b, n); }
static inline const char *strchr(const char *s, int c) { return String::FindChar(s, c); }
static inline long strtol(const char *str, char **endptr, int base) { return String::StrToL(str, endptr, base); }