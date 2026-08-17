#include "TextEditor.hpp"
#include <temrixstd/stdlib.h>
#include "Graphics.hpp"
#include "TermGrid.hpp" 
#include "Keyboard.hpp"

bool TextEditor::Init()
{
    lines = (char (*)[MAX_LINE_LEN])malloc((size_t)MAX_LINES * MAX_LINE_LEN);
    if (!lines) return false;
    lineCount = 1;
    lines[0][0] = '\0';
    cursorLine = cursorCol = scrollTop = 0;
    dirty = false;
    return true;
}

void TextEditor::Destroy()
{
    if (lines) { free(lines); lines = nullptr; }
}

uint32_t TextEditor::LineLen(uint32_t i) const
{
    uint32_t n = 0;
    while (lines[i][n]) n++;
    return n;
}

void TextEditor::Load(FsClient &fs, const char *filePath)
{
    uint32_t i = 0;
    while (filePath[i] && i < sizeof(path) - 1) { path[i] = filePath[i]; i++; }
    path[i] = '\0';

    uint64_t virt = 0;
    uint32_t len = 0;
    lineCount = 0;

    if (fs.readFile(filePath, &virt, &len) && len > 0)
    {
        const char *data = reinterpret_cast<const char *>(virt);
        uint32_t col = 0;
        for (uint32_t k = 0; k < len && lineCount < MAX_LINES - 1; k++)
        {
            char c = data[k];
            if (c == '\n')
            {
                lines[lineCount][col] = '\0';
                lineCount++;
                col = 0;
            }
            else if (col < MAX_LINE_LEN - 1)
            {
                lines[lineCount][col++] = c;
            }
        }
        if (col > 0 || lineCount == 0)
        {
            lines[lineCount][col] = '\0';
            lineCount++;
        }
    }

    if (lineCount == 0) { lines[0][0] = '\0'; lineCount = 1; }
    cursorLine = cursorCol = scrollTop = 0;
    dirty = false;
}

bool TextEditor::Save(FsClient &fs)
{
    uint32_t total = 0;
    for (uint32_t i = 0; i < lineCount; i++) total += LineLen(i) + 1;

    char *buf = (char *)malloc(total > 0 ? total : 1);
    if (!buf) return false;

    uint32_t o = 0;
    for (uint32_t i = 0; i < lineCount; i++)
    {
        uint32_t l = LineLen(i);
        for (uint32_t k = 0; k < l; k++) buf[o++] = lines[i][k];
        if (i + 1 < lineCount) buf[o++] = '\n';
    }

    fs.createFile(path); 
    fs.truncateFile(path, 0);
    FsStatus st = fs.writeFile(path, 0, (const uint8_t *)buf, o);
    free(buf);

    if (st == FsDone) dirty = false;
    return st == FsDone;
}

void TextEditor::InsertChar(char c)
{
    uint32_t len = LineLen(cursorLine);
    if (len >= MAX_LINE_LEN - 1) return;
    for (uint32_t k = len + 1; k > cursorCol; k--)
        lines[cursorLine][k] = lines[cursorLine][k - 1];
    lines[cursorLine][cursorCol] = c;
    cursorCol++;
    dirty = true;
}

void TextEditor::SplitLine()
{
    if (lineCount >= MAX_LINES) return;
    uint32_t len = LineLen(cursorLine);

    for (uint32_t i = lineCount; i > cursorLine + 1; i--)
        for (uint32_t k = 0; k < MAX_LINE_LEN; k++)
            lines[i][k] = lines[i - 1][k];

    uint32_t tailLen = len - cursorCol;
    for (uint32_t k = 0; k < tailLen; k++)
        lines[cursorLine + 1][k] = lines[cursorLine][cursorCol + k];
    lines[cursorLine + 1][tailLen] = '\0';
    lines[cursorLine][cursorCol] = '\0';

    lineCount++;
    cursorLine++;
    cursorCol = 0;
    dirty = true;
}

void TextEditor::Backspace()
{
    if (cursorCol > 0)
    {
        uint32_t len = LineLen(cursorLine);
        for (uint32_t k = cursorCol - 1; k < len; k++)
            lines[cursorLine][k] = lines[cursorLine][k + 1];
        cursorCol--;
        dirty = true;
    }
    else if (cursorLine > 0)
    {
        uint32_t prevLen = LineLen(cursorLine - 1);
        uint32_t curLen = LineLen(cursorLine);
        if (prevLen + curLen >= MAX_LINE_LEN) return;

        for (uint32_t k = 0; k < curLen; k++)
            lines[cursorLine - 1][prevLen + k] = lines[cursorLine][k];
        lines[cursorLine - 1][prevLen + curLen] = '\0';

        for (uint32_t i = cursorLine; i + 1 < lineCount; i++)
            for (uint32_t k = 0; k < MAX_LINE_LEN; k++)
                lines[i][k] = lines[i + 1][k];

        lineCount--;
        cursorLine--;
        cursorCol = prevLen;
        dirty = true;
    }
}

void TextEditor::DeleteForward()
{
    uint32_t len = LineLen(cursorLine);
    if (cursorCol < len)
    {
        for (uint32_t k = cursorCol; k < len; k++)
            lines[cursorLine][k] = lines[cursorLine][k + 1];
        dirty = true;
    }
    else if (cursorLine + 1 < lineCount)
    {
        uint32_t nextLen = LineLen(cursorLine + 1);
        if (len + nextLen >= MAX_LINE_LEN) return;

        for (uint32_t k = 0; k < nextLen; k++)
            lines[cursorLine][len + k] = lines[cursorLine + 1][k];
        lines[cursorLine][len + nextLen] = '\0';

        for (uint32_t i = cursorLine + 1; i + 1 < lineCount; i++)
            for (uint32_t k = 0; k < MAX_LINE_LEN; k++)
                lines[i][k] = lines[i + 1][k];

        lineCount--;
        dirty = true;
    }
}

void TextEditor::MoveLeft()
{
    if (cursorCol > 0) cursorCol--;
    else if (cursorLine > 0) { cursorLine--; cursorCol = LineLen(cursorLine); }
}

void TextEditor::MoveRight()
{
    uint32_t len = LineLen(cursorLine);
    if (cursorCol < len) cursorCol++;
    else if (cursorLine + 1 < lineCount) { cursorLine++; cursorCol = 0; }
}

void TextEditor::MoveUp()
{
    if (cursorLine == 0) return;
    cursorLine--;
    uint32_t len = LineLen(cursorLine);
    if (cursorCol > len) cursorCol = len;
}

void TextEditor::MoveDown()
{
    if (cursorLine + 1 >= lineCount) return;
    cursorLine++;
    uint32_t len = LineLen(cursorLine);
    if (cursorCol > len) cursorCol = len;
}

void TextEditor::MoveHome() { cursorCol = 0; }
void TextEditor::MoveEnd() { cursorCol = LineLen(cursorLine); }

void TextEditor::PageUp(int rows)
{
    cursorLine = ((uint32_t)rows > cursorLine) ? 0 : cursorLine - rows;
    uint32_t len = LineLen(cursorLine);
    if (cursorCol > len) cursorCol = len;
}

void TextEditor::PageDown(int rows)
{
    cursorLine += rows;
    if (cursorLine >= lineCount) cursorLine = lineCount - 1;
    uint32_t len = LineLen(cursorLine);
    if (cursorCol > len) cursorCol = len;
}

static void DrawEditor(uint32_t *buf, uint32_t w, uint32_t h, TextEditor &ed, int visibleRows)
{
    for (uint32_t i = 0; i < w * h; i++) buf[i] = 0xFF15151F;

    if (ed.cursorLine < ed.scrollTop)
        ed.scrollTop = ed.cursorLine;
    if (ed.cursorLine >= ed.scrollTop + (uint32_t)visibleRows)
        ed.scrollTop = ed.cursorLine - (uint32_t)visibleRows + 1;

    for (int r = 0; r < visibleRows; r++)
    {
        uint32_t lineIdx = ed.scrollTop + r;
        if (lineIdx >= ed.lineCount) break;
        Graphics::DrawString(buf, 4, r * CHAR_H, ed.lines[lineIdx], 0xFFE0E0E0, 1, (int)w, (int)h, w);
    }

    
    int cursorScreenRow = (int)(ed.cursorLine - ed.scrollTop);
    int cx = 4 + (int)ed.cursorCol * CHAR_W;
    int cy = cursorScreenRow * CHAR_H;
    for (int y = 0; y < CHAR_H; y++)
        for (int x = 0; x < 2; x++)
            Graphics::PutPixel(buf, cx + x, cy + y, 0xFFFFFFFF, w, h, w);

    
    char status[400];
    uint32_t si = 0;
    auto append = [&](const char *s) { while (*s && si < sizeof(status) - 1) status[si++] = *s++; };
    append(ed.path);
    if (ed.dirty) append("  [modified]");
    append("      ^S Save   ^X Exit");
    status[si] = '\0';

    int statusRow = visibleRows;
    Graphics::DrawString(buf, 4, statusRow * CHAR_H, status, 0xFF80C0FF, 1, (int)w, (int)h, w);
}

void RunTextEditor(FsClient &fs, Window &term, const char *path, KeyEdgeTracker &keyTracker)
{
    TextEditor ed;
    if (!ed.Init()) return;
    ed.Load(fs, path);

    uint32_t w = term.ContentWidth(), h = term.ContentHeight();
    int totalRows = (int)(h / CHAR_H);
    int visibleRows = totalRows > 1 ? totalRows - 1 : 1; 

    auto redraw = [&]()
    {
        term.BeginFrame();
        uint32_t *buf = term.ContentBuffer();
        DrawEditor(buf, w, h, ed, visibleRows);
        term.Present();
    };

    redraw();

    bool running = true;
    while (running)
    {
        bool dirtyFrame = false;

        term.PollInput([&](const WindowInputEvent &ev)
        {
            if (ev.type != InputEventKeyboard) return;

            bool ctrl = (ev.modifierKeys & (MOD_LCTRL | MOD_RCTRL)) != 0;

            keyTracker.Feed(ev.keyCodes, [&](uint32_t code)
            {
                if (ctrl && code == KEY_S) { ed.Save(fs); dirtyFrame = true; return; }
                if (ctrl && code == KEY_X) { running = false; return; }

                switch (code)
                {
                    case KEY_LEFT:       ed.MoveLeft(); break;
                    case KEY_RIGHT:      ed.MoveRight(); break;
                    case KEY_UP:         ed.MoveUp(); break;
                    case KEY_DOWN:       ed.MoveDown(); break;
                    case KEY_HOME:       ed.MoveHome(); break;
                    case KEY_END:        ed.MoveEnd(); break;
                    case KEY_PGUP:       ed.PageUp(visibleRows); break;
                    case KEY_PGDN:       ed.PageDown(visibleRows); break;
                    case KEY_ENTER:      ed.SplitLine(); break;
                    case KEY_BACKSPACE:  ed.Backspace(); break;
                    case KEY_DELETE_FWD: ed.DeleteForward(); break;
                    default:
                    {
                        char ch = KeyCodeToAscii(code, ev.modifierKeys);
                        if (ch) ed.InsertChar(ch);
                        break;
                    }
                }
                dirtyFrame = true;
            });
        });

        if (dirtyFrame) redraw();
    }

    ed.Destroy();
}