#pragma once

#include "efi.hpp"
#include "console.hpp"
#include "framebuffer.hpp"
#include "memorymap.hpp"

struct Input
{
    EfiSimpleTextInputProtocol *in;

    Input() : in(nullptr) {}
    Input(EfiSimpleTextInputProtocol *i) : in(i)
    {
        in->Reset(in, 0);
    }

    bool readKey(EfiInputKey &key)
    {
        return in->ReadKeyStroke(in, &key) == EfiSuccess;
    }

    uint32_t readModeIndex(Console &con, uint32_t defaultVal)
    {
        uint32_t value = 0;
        bool any = false;
        EfiInputKey key;
        uint16_t buf[2];
        buf[1] = 0;

        for (;;)
        {
            if (!readKey(key)) continue;

            if (key.UnicodeChar == u'\r' || key.UnicodeChar == u'\n')
                break;

            if (key.UnicodeChar == 8 && any)
            {
                value /= 10;
                con.print((const uint16_t *)u"\b \b");
                if (value == 0) any = false;
                continue;
            }

            if (key.UnicodeChar >= u'0' && key.UnicodeChar <= u'9')
            {
                value = value * 10 + (key.UnicodeChar - u'0');
                any = true;

                buf[0] = key.UnicodeChar;
                con.print((const uint16_t *)buf);
            }
        }

        return any ? value : defaultVal;
    }

    void echoUntilEscape(Console &con, Framebuffer &fb, MemoryMap &mmap)
    {
        con.printfln("Debug shell, ESC to boot, F1 framebuffer, F2 memory map");

        EfiInputKey key;
        uint16_t buf[2];
        buf[1] = 0;
        while (true)
        {
            if (readKey(key))
            {
                if (key.ScanCode == EfiScanEscape) break;

                switch (key.ScanCode)
                {
                    case EfiScanF1:
                        con.printfln("\r\nFramebuffer base: %llx  size: %llx  res: %llxx%llx",
                                      fb.base,
                                      (uint64_t)fb.pitch * fb.height * 4,
                                      (uint64_t)fb.width, (uint64_t)fb.height);
                        break;

                    case EfiScanF2:
                        con.printfln("\r\nMemory map:");
                        dumpMemoryMap(con, mmap);
                        break;

                    default:
                        buf[0] = key.UnicodeChar;
                        con.print(buf);
                        break;
                }
            }
        }
        con.println((const uint16_t *)u"");
    }

private:
    void dumpMemoryMap(Console &con, MemoryMap &mmap)
    {
        uint32_t count = mmap.size / mmap.descSize;
        for (uint32_t i = 0; i < count; i++)
        {
            uint8_t  *desc  = (uint8_t *)mmap.buf + i * mmap.descSize;
            uint32_t  type  = *(uint32_t *)(desc + 0);
            uint64_t  addr  = *(uint64_t *)(desc + 8);
            uint64_t  pages = *(uint64_t *)(desc + 24);

            con.printfln("  [%llx] addr: %llx pages: %llx", (uint64_t)type, addr, pages);
        }
    }
};