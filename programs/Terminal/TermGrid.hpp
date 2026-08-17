#pragma once
#include <temrixstd.h>

static constexpr int CHAR_W = 8;
static constexpr int CHAR_H = 16;

struct TermGrid
{
    static constexpr int MAX_COLS = 256, MAX_ROWS = 128;
    char cells[MAX_ROWS][MAX_COLS];
    int cols = 0, rows = 0;
    int cursorRow = 0, cursorCol = 0;

    void Init(uint32_t pxW, uint32_t pxH);
    void Put(char ch);
    void Backspace();
    void NewLine();
    void Print(const char *s);
};

void RedrawLine(uint32_t *buf, uint32_t w, uint32_t h, TermGrid &grid, int row);
