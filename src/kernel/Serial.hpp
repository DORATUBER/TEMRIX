#pragma once

#include "string.hpp"
#include "common.hpp"
#include "console.hpp"

namespace Serial {
    void init(Graphics::FrameBuffer fb);
    void putc(char c);
    void print(const char* str);
    void printf(const char* fmt, ...);
    void render(); 
}