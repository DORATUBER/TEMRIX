#pragma once
#include <temrixstd.h>
#include "Window.hpp"
#include "FileSystem/FsClient.hpp"
#include "TermGrid.hpp"
#include "Keyboard.hpp"

void RunCommand(FsClient &fs, char *cwd, const char *cmdIn, TermGrid &grid, bool &fullRedraw, Window &term, KeyEdgeTracker &keyTracker);