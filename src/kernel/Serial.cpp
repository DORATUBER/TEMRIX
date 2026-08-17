#include "Serial.hpp"
#include "CharBuffer.hpp"
#include "console.hpp"
#include "io.hpp"

#define DEBUG

namespace Serial {
    constexpr uint16_t COM1 = 0x3F8;

    static CharBuffer *g_charBuf = nullptr;
    static Graphics::TextConsole *g_console = nullptr;

    void init(Graphics::FrameBuffer fb) {
        g_charBuf = new CharBuffer();
        char *buf = (char *)new uint8_t[16 * 1024];
        g_charBuf->initBuffer(buf, 16 * 1024);

        g_console = new Graphics::TextConsole(nullptr, fb);
        g_console->setBuffer(g_charBuf);

        #ifdef DEBUG
        outb(COM1 + 1, 0x00);
        outb(COM1 + 3, 0x80);
        outb(COM1 + 0, 0x03);
        outb(COM1 + 1, 0x00);
        outb(COM1 + 3, 0x03);
        outb(COM1 + 2, 0xC7);
        outb(COM1 + 4, 0x03);
        #endif
    }

    void putc(char c) {
        g_charBuf->writeByte(c);
        #ifdef DEBUG
        while (!(inb(COM1 + 5) & 0x20));
        outb(COM1, c);
        #endif
    }

    void print(const char* str) {
        while (*str) putc(*str++);
    }

    void printf(const char* fmt, ...) {
        char buf[512];
        __builtin_va_list args;
        __builtin_va_start(args, fmt);
        String::kvsnprintf(buf, sizeof(buf), fmt, args);
        __builtin_va_end(args);
        print(buf);
    }

    void render() {
        if (g_console) g_console->render();
    }
}