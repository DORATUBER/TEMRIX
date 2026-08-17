#include "console.hpp"

namespace Graphics
{
    TextConsole::TextConsole(Memory::VMM *vmm,
                             FrameBuffer &screen,
                             int layer_x,
                             int layer_y)
        : m_cols(screen.Width / GLYPH_W), m_rows(screen.Height / GLYPH_H)
    {
        m_layer = screen;
        clearLayer();
    }

    void TextConsole::setBuffer(CharBuffer *buf) { m_buf = buf; }

    void TextConsole::clearLayer()
    {
        drawRect(m_layer, 0, 0, m_layer.Width, m_layer.Height, bg);
    }

    void TextConsole::clear()
    {
        clearLayer();
        if (m_buf)
            m_buf->clear();
    }

    void TextConsole::renderChar(int col, int row, char c, uint32_t color)
    {
        if (col < 0 || col >= m_cols || row < 0 || row >= m_rows)
            return;
        int px = col * GLYPH_W;
        int py = row * GLYPH_H;
        drawRect(m_layer, px, py, GLYPH_W, GLYPH_H, bg);
        Graphics::putChar(m_layer, c, px, py, color);
    }

    void TextConsole::clearLine(int row)
    {
        if (row < 0 || row >= m_rows)
            return;
        drawRect(m_layer, 0, row * GLYPH_H, m_layer.Width, GLYPH_H, bg);
    }

    void TextConsole::clearToEndOfLine(int col, int row)
    {
        if (row < 0 || row >= m_rows)
            return;
        int px = col * GLYPH_W;
        drawRect(m_layer, px, row * GLYPH_H, m_layer.Width - px, GLYPH_H, bg);
    }

    static int readArgs(const CharBuffer *buf, size_t start, size_t i,
                        size_t count, char *args, int maxArgs)
    {
        int n = 0;
        while (i < count && n < maxArgs)
        {
            char b = buf->data[(start + i) % buf->capacity];
            if (b == '\x02')
                break;
            args[n++] = b;
            i++;
        }
        return n;
    }

    static int commandSkip(const CharBuffer *buf, size_t start,
                           size_t i, size_t count)
    {
        int n = 0;
        while (i < count)
        {
            char b = buf->data[(start + i) % buf->capacity];
            n++;
            if (b == '\x02')
                break;
            i++;
        }
        return n;
    }

    static int countRows(const CharBuffer *buf, int cols)
    {
        size_t start = buf->readStart();
        size_t count = buf->readLen();
        int cx = 0, cy = 0;

        for (size_t i = 0; i < count; i++)
        {
            char c = buf->data[(start + i) % buf->capacity];

            if (c == '\x01')
            {
                i += commandSkip(buf, start, i + 1, count);
                continue;
            }

            switch (c)
            {
            case '\n':
                cx = 0;
                cy++;
                break;
            case '\r':
                cx = 0;
                break;
            case '\t':
                cx = (cx + 8) & ~7;
                if (cx >= cols)
                {
                    cx = 0;
                    cy++;
                }
                break;
            default:
                cx++;
                if (cx >= cols)
                {
                    cx = 0;
                    cy++;
                }
                break;
            }
        }
        return cy + 1;
    }

    void TextConsole::render()
    {
        if (!m_buf || !m_buf->data)
            return;

        clearLayer();

        size_t start = m_buf->readStart();
        size_t count = m_buf->readLen();
        int total = countRows(m_buf, m_cols);
        int offset = total > m_rows ? total - m_rows : 0;
        uint32_t color = fg;
        int cx = 0, cy = 0;

        for (size_t i = 0; i < count; i++)
        {
            char c = m_buf->data[(start + i) % m_buf->capacity];
            if (c == '\x01')
            {
                i++; 
                if (i >= count)
                    break;
                char cmd = m_buf->data[(start + i) % m_buf->capacity];
                i++; 

                
                char args[8] = {};
                int nargs = 0;
                while (i < count && nargs < 8)
                {
                    char b = m_buf->data[(start + i) % m_buf->capacity];
                    i++;
                    if (b == '\x02')
                        break;
                    args[nargs++] = b;
                }
                i--; 

                switch (cmd)
                {
                case 'c': 
                    if (nargs >= 4)
                        color = ((uint32_t)(uint8_t)args[3] << 24) |
                                ((uint32_t)(uint8_t)args[0] << 16) |
                                ((uint32_t)(uint8_t)args[1] << 8) |
                                (uint32_t)(uint8_t)args[2];
                    break;

                case 'r': 
                    color = fg;
                    break;

                case 'u': 
                    if (nargs >= 1)
                        cy -= (uint8_t)args[0];
                    if (cy < 0)
                        cy = 0;
                    break;

                case 'd': 
                    if (nargs >= 1)
                        cy += (uint8_t)args[0];
                    break;

                case 'p': 
                    if (nargs >= 2)
                    {
                        cx = (uint8_t)args[0];
                        cy = (uint8_t)args[1];
                    }
                    break;

                case 'k': 
                    clearToEndOfLine(cx, cy - offset);
                    break;

                case 'K': 
                    clearLine(cy - offset);
                    cx = 0;
                    break;

                case 's': 
                    clearLayer();
                    cx = 0;
                    cy = offset; 
                    break;
                }
                continue;
            }
            switch (c)
            {
            case '\n':
                cx = 0;
                cy++;
                break;
            case '\r':
                cx = 0;
                break;
            case '\t':
                cx = (cx + 8) & ~7;
                if (cx >= m_cols)
                {
                    cx = 0;
                    cy++;
                }
                break;
            default:
                if (cy >= offset && (cy - offset) < m_rows)
                    renderChar(cx, cy - offset, c, color);
                cx++;
                if (cx >= m_cols)
                {
                    cx = 0;
                    cy++;
                }
                break;
            }
        }
    }
}