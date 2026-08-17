#pragma once
#include "common.hpp"
#include "kernel/string.hpp"

struct CharBuffer {
    char*  data;
    size_t capacity;
    size_t head;
    bool   wrapped;

    void initBuffer(char *buf, size_t cap) {
        data     = buf;
        capacity = cap;
        head     = 0;
        wrapped  = false;
    }

    void clear() { head = 0; wrapped = false; }

    void writeByte(char c) {
        data[head++] = c;
        if (head >= capacity) { head = 0; wrapped = true; }
    }

    void write(const char* str) {
        while (*str) writeByte(*str++);
    }

    void printf(const char* fmt, ...) {
        char buf[1024];
        __builtin_va_list args;
        __builtin_va_start(args, fmt);
        String::kvsnprintf(buf, sizeof(buf), fmt, args);
        __builtin_va_end(args);
        write(buf);
    }

    void setColor(uint32_t color) {
        writeByte('\x01'); writeByte('c');
        writeByte((char)((color >> 16) & 0xFF)); 
        writeByte((char)((color >>  8) & 0xFF)); 
        writeByte((char)((color >>  0) & 0xFF)); 
        writeByte((char)((color >> 24) & 0xFF)); 
        writeByte('\x02');
    }

    void resetColor() {
        writeByte('\x01'); writeByte('r'); writeByte('\x02');
    }

    void cursorUp(uint8_t n) {
        writeByte('\x01'); writeByte('u'); writeByte((char)n); writeByte('\x02');
    }

    void cursorDown(uint8_t n) {
        writeByte('\x01'); writeByte('d'); writeByte((char)n); writeByte('\x02');
    }

    void moveCursor(uint8_t x, uint8_t y) {
        writeByte('\x01'); writeByte('p');
        writeByte((char)x); writeByte((char)y);
        writeByte('\x02');
    }

    void clearToEOL() {
        writeByte('\x01'); writeByte('k'); writeByte('\x02');
    }

    void clearLine() {
        writeByte('\x01'); writeByte('K'); writeByte('\x02');
    }

    void clearScreen() {
        writeByte('\x01'); writeByte('s'); writeByte('\x02');
    }

    size_t readStart() const { return wrapped ? head : 0; }
    size_t readLen()   const { return wrapped ? capacity : head; }
};