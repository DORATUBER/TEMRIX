#pragma once
#include "graphics.hpp"
#include "vmm.hpp"
#include "CharBuffer.hpp"

namespace Graphics
{
    class TextConsole
    {
    public:
        static constexpr int GLYPH_W = 8;
        static constexpr int GLYPH_H = 16;

        uint32_t fg = 0xFFCCCCCC;
        uint32_t bg = 0x00000000;

        TextConsole(Memory::VMM *vmm,
                    FrameBuffer &screen,
                    int layer_x = 0,
                    int layer_y = 0);

        void setBuffer(CharBuffer *buf);
        void render();
        void clear();
    private:
        FrameBuffer m_layer;
        CharBuffer *m_buf = nullptr;
        int m_cols, m_rows;

        void renderChar(int col, int row, char c, uint32_t color);
        void clearLine(int row);
        void clearToEndOfLine(int col, int row);
        void clearLayer();
    };
} 