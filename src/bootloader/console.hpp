#pragma once
#include "efi.hpp"
#include "string.hpp"

struct Console
{
    EfiSimpleTextOutputProtocol *out;

    Console() : out(nullptr) {}
    Console(EfiSimpleTextOutputProtocol *o) : out(o)
    {
        out->SetAttribute(out, EfiWhite);
        out->ClearScreen(out);
    }

    void print(const uint16_t *s)
    {
        out->OutputString(out, s);
    }

    void println(const uint16_t *s)
    {
        print(s);
        print((const uint16_t *)u"\r\n");
    }

    void printHex8(uint8_t value)
    {
        uint16_t buffer[3];
        buffer[2] = 0;
        const char *hex = "0123456789ABCDEF";
        for (int8_t i = 1; i >= 0; i--)
        {
            buffer[i] = (uint16_t)hex[value & 0xF];
            value >>= 4;
        }
        print(buffer);
    }

    void printHex64(uint64_t value)
    {
        uint16_t buffer[17];
        buffer[16] = 0;
        const char *hex = "0123456789ABCDEF";
        for (int32_t i = 15; i >= 0; i--)
        {
            buffer[i] = (uint16_t)hex[value & 0xF];
            value >>= 4;
        }
        print(buffer);
    }

    void printHexN(const uint8_t *value, uint32_t length)
    {
        while (length--)
            printHex8(*value++);
    }

    void printHex64Label(const uint16_t *label, uint64_t value)
    {
        print(label);
        printHex64(value);
        print((const uint16_t *)u"\r\n");
    }

    void printDec(uint64_t value)
    {
        uint16_t buffer[21]; 
        int32_t i = 20;
        buffer[i] = 0;

        if (value == 0)
        {
            buffer[--i] = u'0';
        }
        else
        {
            while (value > 0)
            {
                buffer[--i] = u'0' + (uint16_t)(value % 10);
                value /= 10;
            }
        }

        print(&buffer[i]);
    }

    void printDecLabel(const uint16_t *label, uint64_t value)
    {
        print(label);
        printDec(value);
        print((const uint16_t *)u"\r\n");
    }

    void printf(const char *fmt, ...)
    {
        char buf[256];

        __builtin_va_list args;
        __builtin_va_start(args, fmt);
        String::kvsnprintf(buf, sizeof(buf), fmt, args);
        __builtin_va_end(args);

        print(buf);
    }

    void printfln(const char *fmt, ...)
    {
        char buf[256];

        __builtin_va_list args;
        __builtin_va_start(args, fmt);
        String::kvsnprintf(buf, sizeof(buf), fmt, args);
        __builtin_va_end(args);

        print(buf);
        print((const uint16_t *)u"\r\n");
    }

private:
    void print(const char *s)
    {
        uint16_t wbuf[256];
        int i = 0;
        for (; s[i] != '\0' && i < 255; i++)
            wbuf[i] = (uint16_t)(unsigned char)s[i];
        wbuf[i] = 0;

        out->OutputString(out, wbuf);
    }
};