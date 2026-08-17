#pragma once
#include <temrixstd.h>
#include "Window.hpp"
#include "FileSystem/FsClient.hpp"
#include "Keyboard.hpp"

struct TextEditor
{
    static constexpr uint32_t MAX_LINES = 4000;
    static constexpr uint32_t MAX_LINE_LEN = 256;

    char (*lines)[MAX_LINE_LEN] = nullptr;
    uint32_t lineCount = 0;
    uint32_t cursorLine = 0, cursorCol = 0;
    uint32_t scrollTop = 0;
    bool dirty = false;
    char path[512] = {0};

    bool Init();
    void Destroy();
    uint32_t LineLen(uint32_t i) const;

    void Load(FsClient &fs, const char *filePath);
    bool Save(FsClient &fs);

    void InsertChar(char c);
    void SplitLine();
    void Backspace();
    void DeleteForward();

    void MoveLeft();
    void MoveRight();
    void MoveUp();
    void MoveDown();
    void MoveHome();
    void MoveEnd();
    void PageUp(int rows);
    void PageDown(int rows);
};

void RunTextEditor(FsClient &fs, Window &term, const char *path, KeyEdgeTracker &keyTracker);