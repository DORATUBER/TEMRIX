#include "graphics.hpp"
#include "kernel/string.hpp"

namespace Graphics
{
    void putPixel(FrameBuffer fb, int x, int y, uint32_t color)
    {
        if (x < 0 || x >= (int)fb.Width || y < 0 || y >= (int)fb.Height)
            return;
        fb.data[y * fb.PixelsPerScanLine + x] = color;
    }

    uint32_t getPixel(FrameBuffer fb, int x, int y)
    {
        if (x < 0 || x >= (int)fb.Width || y < 0 || y >= (int)fb.Height)
            return 0;
        return fb.data[y * fb.PixelsPerScanLine + x]; 
    }

    uint32_t blendPixels(uint32_t src, uint32_t dst)
    {
        uint8_t alpha = (src >> 24) & 0xFF;
        if (alpha == 255)
            return src;
        if (alpha == 0)
            return dst;

        uint8_t inv_alpha = 255 - alpha;
        uint8_t r = (((src >> 16) & 0xFF) * alpha + ((dst >> 16) & 0xFF) * inv_alpha) >> 8;
        uint8_t g = (((src >> 8) & 0xFF) * alpha + ((dst >> 8) & 0xFF) * inv_alpha) >> 8;
        uint8_t b = ((src & 0xFF) * alpha + (dst & 0xFF) * inv_alpha) >> 8;
        return (0xFF << 24) | (r << 16) | (g << 8) | b;
    }

    uint32_t blendPixelsWeighted(uint32_t src, uint32_t dst, uint8_t weight)
    {
        uint32_t alpha = (((src >> 24) & 0xFF) * weight) >> 8;
        if (alpha >= 255)
            return src;
        if (alpha == 0)
            return dst;

        uint8_t inv_alpha = 255 - alpha;
        uint8_t r = (((src >> 16) & 0xFF) * alpha + ((dst >> 16) & 0xFF) * inv_alpha) >> 8;
        uint8_t g = (((src >> 8) & 0xFF) * alpha + ((dst >> 8) & 0xFF) * inv_alpha) >> 8;
        uint8_t b = ((src & 0xFF) * alpha + (dst & 0xFF) * inv_alpha) >> 8;
        return (0xFF << 24) | (r << 16) | (g << 8) | b;
    }

    void drawRect(FrameBuffer fb, int x, int y, int w, int h, uint32_t color)
    {
        for (int i = 0; i < h; i++)
            for (int j = 0; j < w; j++)
                putPixel(fb, x + j, y + i, color);
    }

    void drawRectBlended(FrameBuffer fb, int x, int y, int w, int h, uint32_t color)
    {
        uint8_t alpha = (color >> 24) & 0xFF;
        uint8_t inv_alpha = 255 - alpha;
        uint8_t sr = (color >> 16) & 0xFF;
        uint8_t sg = (color >> 8) & 0xFF;
        uint8_t sb = color & 0xFF;

        for (int i = 0; i < h; i++)
            for (int j = 0; j < w; j++)
            {
                uint32_t dst = getPixel(fb, x + j, y + i);
                uint8_t r = (sr * alpha + ((dst >> 16) & 0xFF) * inv_alpha) >> 8;
                uint8_t g = (sg * alpha + ((dst >> 8) & 0xFF) * inv_alpha) >> 8;
                uint8_t b = (sb * alpha + (dst & 0xFF) * inv_alpha) >> 8;
                putPixel(fb, x + j, y + i, (r << 16) | (g << 8) | b);
            }
    }

    static inline bool isCornerClipped(int j, int i, int w, int h, int r)
    {
        int cx = -1, cy = -1;

        
        if (j < r && i < r)
        {
            cx = r - j - 1;
            cy = r - i - 1;
        }
        
        else if (j >= w - r && i < r)
        {
            cx = j - (w - r);
            cy = r - i - 1;
        }
        
        else if (j < r && i >= h - r)
        {
            cx = r - j - 1;
            cy = i - (h - r);
        }
        
        else if (j >= w - r && i >= h - r)
        {
            cx = j - (w - r);
            cy = i - (h - r);
        }

        return (cx >= 0 && cy >= 0 && (cx * cx + cy * cy > r * r));
    }

    void drawRoundedRect(FrameBuffer fb,
                         int x, int y,
                         int w, int h,
                         int r,
                         uint32_t color)
    {
        for (int i = 0; i < h; i++)
        {
            for (int j = 0; j < w; j++)
            {
                if (!isCornerClipped(j, i, w, h, r))
                {
                    putPixel(fb, x + j, y + i, color);
                }
            }
        }
    }

    void drawRoundedRectOutline(FrameBuffer fb,
                                int x, int y,
                                int w, int h,
                                int r,
                                uint32_t color)
    {
        for (int i = 0; i < h; i++)
        {
            for (int j = 0; j < w; j++)
            {
                if (isCornerClipped(j, i, w, h, r))
                    continue;

                
                if (i == 0 || i == h - 1 || j == 0 || j == w - 1)
                {
                    putPixel(fb, x + j, y + i, color);
                }
            }
        }
    }

    void swapBuffer(FrameBuffer back, FrameBuffer front)
    {
#if HAS_UINT128
        uint128_t *src = (uint128_t *)back.data;
        uint128_t *dst = (uint128_t *)front.data;
        uint32_t count = (back.PixelsPerScanLine * back.Height * sizeof(uint32_t)) / 16;
#else
        uint64_t *src = (uint64_t *)back.data;
        uint64_t *dst = (uint64_t *)front.data;
        uint32_t count = (back.PixelsPerScanLine * back.Height * sizeof(uint32_t)) / 8;
#endif
        for (uint32_t i = 0; i < count; i++)
            dst[i] = src[i];
    }

    void putChar(FrameBuffer fb, char c, int x, int y, uint32_t color)
    {
        unsigned char *glyph = (unsigned char *)&fontdata_8x16[(unsigned char)c * 16];
        for (int r = 0; r < 16; r++)
            for (int col = 0; col < 8; col++)
                if (glyph[r] & (0x80 >> col))
                    putPixel(fb, x + col, y + r, color);
    }

    void putCharScaled(FrameBuffer fb, char c, int x, int y, uint32_t color, int scale)
    {
        unsigned char *glyph = (unsigned char *)&fontdata_8x16[(unsigned char)c * 16];
        for (int r = 0; r < 16; r++)
            for (int col = 0; col < 8; col++)
                if (glyph[r] & (0x80 >> col))
                    for (int sy = 0; sy < scale; sy++)
                        for (int sx = 0; sx < scale; sx++)
                            putPixel(fb, x + col * scale + sx, y + r * scale + sy, color);
    }

    void print(FrameBuffer fb, const char *str, int x, int y, uint32_t color)
    {
        for (int i = 0; str[i] != '\0'; i++)
            putChar(fb, str[i], x + (i * 8), y, color);
    }

    void printScaled(FrameBuffer fb, const char *str, int x, int y, uint32_t color, int scale)
    {
        for (int i = 0; str[i] != '\0'; i++)
            putCharScaled(fb, str[i], x + (i * 8 * scale), y, color, scale);
    }

    void printHex(FrameBuffer fb, uint32_t val, int x, int y, uint32_t color)
    {
        char hex_chars[] = "0123456789ABCDEF";
        char buffer[11];
        buffer[0] = '0';
        buffer[1] = 'x';
        buffer[10] = '\0';
        for (int i = 9; i >= 2; i--)
        {
            buffer[i] = hex_chars[val & 0xF];
            val >>= 4;
        }
        print(fb, buffer, x, y, color);
    }

    void printf(FrameBuffer fb, int x, int y, uint32_t color, const char *fmt, ...)
    {
        char buf[1024];
        __builtin_va_list args;
        __builtin_va_start(args, fmt);

        String::kvsnprintf(buf, sizeof(buf), fmt, args);
        __builtin_va_end(args);
        print(fb, buf, x, y, color);
    }
}