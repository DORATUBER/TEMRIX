#include <temrixstd.h>
#include <temrixstd/stdlib.h>
#include "Window.hpp"
#include "FileSystem/FsClient.hpp"
#include "Graphics.hpp"
#include "Keyboard.hpp"
#include "TermGrid.hpp" 
#include "Shell.hpp"

int main(int argc, char **argv)
{
    FramebufferInfo fbInfo{};
    Syscall::Info::Get(InfoFramebuffer, &fbInfo);

    WindowOptions opts;
    opts.decorated = false;
    opts.fullscreen = true;
    opts.background = true; 

    Window term;
    if (!term.Init(0, 0, fbInfo.width, fbInfo.height, "", {}, opts))
        return -1;

    FsClient fs;
    fs.init();

    TermGrid grid;
    grid.Init(term.ContentWidth(), term.ContentHeight());

    KeyEdgeTracker keyTracker;

    char cwd[256] = "/";
    char lineBuf[256] = {0};
    uint32_t lineLen = 0;
    bool fullRedraw = false;

    bool haveCommand = false;
    char pendingCmd[256] = {0};

    grid.Print("temrix$ ");
    term.BeginFrame();
    {
        uint32_t *buf = term.ContentBuffer();
        uint32_t w = term.ContentWidth(), h = term.ContentHeight();
        for (int r = 0; r < grid.rows; r++)
            RedrawLine(buf, w, h, grid, r);
    }
    term.Present();

    while (true)
    {
        bool dirty = false;
        int dirtyRow = grid.cursorRow;

        term.PollInput([&](const WindowInputEvent &ev)
        {
            if (ev.type != InputEventKeyboard) return; 

            keyTracker.Feed(ev.keyCodes, [&](uint32_t code)
            {
                if (code == KEY_ENTER)
                {
                    grid.NewLine();
                    uint32_t i = 0;
                    while (lineBuf[i] && i < sizeof(pendingCmd) - 1) { pendingCmd[i] = lineBuf[i]; i++; }
                    pendingCmd[i] = '\0';
                    haveCommand = true;
                    lineLen = 0; lineBuf[0] = '\0';
                }
                else if (code == KEY_BACKSPACE)
                {
                    if (lineLen > 0) { lineBuf[--lineLen] = '\0'; grid.Backspace(); }
                }
                else
                {
                    char ch = KeyCodeToAscii(code, ev.modifierKeys);
                    if (ch && lineLen < 255) { lineBuf[lineLen++] = ch; lineBuf[lineLen] = '\0'; grid.Put(ch); }
                }
                dirty = true;
            });
        });

        if (haveCommand)
        {
            haveCommand = false;
            RunCommand(fs, cwd, pendingCmd, grid, fullRedraw, term, keyTracker);
            grid.Print(cwd);
            grid.Print("$ ");
            dirty = true;
        }

        if (dirty)
        {
            term.BeginFrame();
            uint32_t *buf = term.ContentBuffer();
            uint32_t w = term.ContentWidth(), h = term.ContentHeight();

            if (fullRedraw || dirtyRow != grid.cursorRow)
            {
                for (int r = 0; r < grid.rows; r++)
                    RedrawLine(buf, w, h, grid, r);
                term.PresentRect(0, 0, w, h);
            }
            else
            {
                RedrawLine(buf, w, h, grid, grid.cursorRow);
                term.PresentRect(0, dirtyRow * CHAR_H, w, CHAR_H);
            }

            dirty = false;
            fullRedraw = false;
        }
    }

    return 0;
}