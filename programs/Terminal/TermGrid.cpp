#include "TermGrid.hpp"
#include "Graphics.hpp"

void TermGrid::Init(uint32_t pxW, uint32_t pxH)
{
    cols = pxW / CHAR_W;
    rows = pxH / CHAR_H;
    cursorRow = 0;
    cursorCol = 0;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            cells[r][c] = ' ';
}

void TermGrid::Put(char ch)
{
    if (ch == '\n' || cursorCol >= cols) { NewLine(); if (ch == '\n') return; }
    cells[cursorRow][cursorCol++] = ch;
}

void TermGrid::Backspace()
{
    if (cursorCol > 0) cells[cursorRow][--cursorCol] = ' ';
}

void TermGrid::NewLine()
{
    cursorCol = 0;
    cursorRow++;
    if (cursorRow >= rows)
    {
        for (int r = 1; r < rows; r++)
            for (int c = 0; c < cols; c++)
                cells[r - 1][c] = cells[r][c];
        for (int c = 0; c < cols; c++)
            cells[rows - 1][c] = ' ';
        cursorRow = rows - 1;
    }
}

void TermGrid::Print(const char *s) { while (*s) Put(*s++); }

void RedrawLine(uint32_t *buf, uint32_t w, uint32_t h, TermGrid &grid, int row)
{
    for (uint32_t px = 0; px < w; px++)
        for (uint32_t py = row * CHAR_H; py < (uint32_t)(row + 1) * CHAR_H; py++)
            if (py < h) buf[py * w + px] = 0xFF15151F; 

    char line[TermGrid::MAX_COLS + 1];
    int c = 0;
    for (; c < grid.cols; c++) line[c] = grid.cells[row][c];
    line[c] = '\0';

    Graphics::DrawString(buf, 4, row * CHAR_H, line, 0xFFE0E0E0, 1, (int)w, (int)h, w);
}
