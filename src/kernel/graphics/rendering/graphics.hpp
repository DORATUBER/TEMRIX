#pragma once
#include "common.hpp"
#include "font.hpp"

#define COLOR(r, g, b, a) ((uint32_t)((a) << 24 | (r) << 16 | (g) << 8 | (b)))

namespace Graphics {
    struct FrameBuffer {
        uint32_t* data;
        uint32_t  Width;
        uint32_t  Height;
        uint32_t  PixelsPerScanLine;
    };

    void putPixel(FrameBuffer fb, int x, int y, uint32_t color);
    uint32_t getPixel(FrameBuffer fb, int x, int y);
    uint32_t blendPixels(uint32_t src, uint32_t dst);
    uint32_t blendPixelsWeighted(uint32_t src, uint32_t dst, uint8_t weight);

    void drawRect(FrameBuffer fb, int x, int y, int w, int h, uint32_t color);
    void drawRectBlended(FrameBuffer fb, int x, int y, int w, int h, uint32_t color);
    void drawRoundedRect(FrameBuffer fb, int x, int y, int w, int h, int r, uint32_t color);
    void drawRoundedRectOutline(FrameBuffer fb, int x, int y, int w, int h, int r, uint32_t color);
    void drawBootSplash(Graphics::FrameBuffer& fb);
    void swapBuffer(FrameBuffer back, FrameBuffer front);

    void putChar(FrameBuffer fb, char c, int x, int y, uint32_t color);
    void putCharScaled(FrameBuffer fb, char c, int x, int y, uint32_t color, int scale);
    void print(FrameBuffer fb, const char* str, int x, int y, uint32_t color);
    void printScaled(FrameBuffer fb, const char* str, int x, int y, uint32_t color, int scale);
    void printHex(FrameBuffer fb, uint32_t val, int x, int y, uint32_t color);
    void printf(FrameBuffer fb, int x, int y, uint32_t color, const char* fmt, ...);
}