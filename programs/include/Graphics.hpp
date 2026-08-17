#pragma once
#include <temrixstd.h>
#include "font.hpp"

namespace Graphics
{
    static void PutPixel(uint32_t *fb, int x, int y, uint32_t color, int sw, int sh, uint32_t pps)
    {
        if (x >= 0 && y >= 0 && x < sw && y < sh)
            fb[y * pps + x] = color;
    }

    static void DrawRect(uint32_t *fb, int x, int y, int w, int h, uint32_t color, int sw, int sh, uint32_t pps)
    {
        for (int i = 0; i < h; i++)
            for (int j = 0; j < w; j++)
                PutPixel(fb, x + j, y + i, color, sw, sh, pps);
    }

    static void DrawGlyph(uint32_t *fb, int x, int y, char c, uint32_t color, int scale, int sw, int sh, uint32_t pps)
    {
        const uint8_t *glyph = &Graphics::fontdata_8x16[(uint8_t)c * 16];
        for (int row = 0; row < 16; row++)
        {
            for (int col = 0; col < 8; col++)
            {
                if (glyph[row] & (0x80 >> col))
                {
                    for (int sy = 0; sy < scale; sy++)
                        for (int sx = 0; sx < scale; sx++)
                            PutPixel(fb, x + (col * scale) + sx, y + (row * scale) + sy, color, sw, sh, pps);
                }
            }
        }
    }

    static void DrawString(uint32_t *fb, int x, int y, const char *str, uint32_t color, int scale, int sw, int sh, uint32_t pps)
    {
        for (int i = 0; str[i]; i++)
        {
            DrawGlyph(fb, x + (i * 8 * scale), y, str[i], color, scale, sw, sh, pps);
        }
    }

    static uint32_t Blend(uint32_t src, uint32_t dst)
    {
        uint8_t a = (src >> 24) & 0xFF;
        if (a == 255)
            return src;
        if (a == 0)
            return dst;
        uint8_t ia = 255 - a;
        uint8_t r = (((src >> 16) & 0xFF) * a + ((dst >> 16) & 0xFF) * ia) >> 8;
        uint8_t g = (((src >> 8) & 0xFF) * a + ((dst >> 8) & 0xFF) * ia) >> 8;
        uint8_t b = (((src >> 0) & 0xFF) * a + ((dst >> 0) & 0xFF) * ia) >> 8;
        return (0xFF << 24) | (r << 16) | (g << 8) | b;
    }
}