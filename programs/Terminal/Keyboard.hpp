#pragma once
#include <temrixstd.h>

enum : uint32_t
{
    KEY_A = 0x04, 
    KEY_S = 0x16, 
    KEY_X = 0x1B, 
    KEY_1 = 0x1E, 
    KEY_0 = 0x27,
    KEY_ENTER = 0x28,
    KEY_ESC = 0x29,
    KEY_BACKSPACE = 0x2A,
    KEY_TAB = 0x2B,
    KEY_SPACE = 0x2C,
    KEY_MINUS = 0x2D,
    KEY_EQUALS = 0x2E,
    KEY_LBRACKET = 0x2F,
    KEY_RBRACKET = 0x30,
    KEY_BACKSLASH = 0x31,
    KEY_SEMICOLON = 0x33,
    KEY_QUOTE = 0x34,
    KEY_GRAVE = 0x35,
    KEY_COMMA = 0x36,
    KEY_PERIOD = 0x37,
    KEY_SLASH = 0x38,

    KEY_DELETE_FWD = 0x4C,
    KEY_HOME = 0x4A,
    KEY_PGUP = 0x4B,
    KEY_END = 0x4D,
    KEY_PGDN = 0x4E,
    KEY_RIGHT = 0x4F,
    KEY_LEFT = 0x50,
    KEY_DOWN = 0x51,
    KEY_UP = 0x52,
};

enum : uint32_t
{
    MOD_LCTRL = 0x01,
    MOD_LSHIFT = 0x02,
    MOD_LALT = 0x04,
    MOD_LGUI = 0x08,
    MOD_RCTRL = 0x10,
    MOD_RSHIFT = 0x20,
    MOD_RALT = 0x40,
    MOD_RGUI = 0x80,
};

inline char KeyCodeToAscii(uint32_t code, uint32_t modifiers)
{
    bool shift = (modifiers & (MOD_LSHIFT | MOD_RSHIFT)) != 0;

    if (code >= KEY_A && code <= 0x1D) 
    {
        char base = 'a' + (char)(code - KEY_A);
        return shift ? (base - 'a' + 'A') : base;
    }

    if (code >= KEY_1 && code <= 0x26) 
    {
        static const char shifted[] = "!@#$%^&*(";
        char base = '1' + (char)(code - KEY_1);
        return shift ? shifted[code - KEY_1] : base;
    }

    if (code == KEY_0)
        return shift ? ')' : '0';

    switch (code)
    {
        case KEY_SPACE:     return ' ';
        case KEY_MINUS:     return shift ? '_' : '-';
        case KEY_EQUALS:    return shift ? '+' : '=';
        case KEY_LBRACKET:  return shift ? '{' : '[';
        case KEY_RBRACKET:  return shift ? '}' : ']';
        case KEY_BACKSLASH: return shift ? '|' : '\\';
        case KEY_SEMICOLON: return shift ? ':' : ';';
        case KEY_QUOTE:     return shift ? '"' : '\'';
        case KEY_GRAVE:     return shift ? '~' : '`';
        case KEY_COMMA:     return shift ? '<' : ',';
        case KEY_PERIOD:    return shift ? '>' : '.';
        case KEY_SLASH:     return shift ? '?' : '/';
        default:            return 0; 
    }
}

class KeyEdgeTracker
{
public:
    template <typename OnPress>
    void Feed(const uint32_t (&codes)[6], OnPress &&onPress)
    {
        for (int i = 0; i < 6; i++)
        {
            uint32_t code = codes[i];
            if (!code) continue;
            if (!WasDown(code))
                onPress(code);
        }

        for (int i = 0; i < 6; i++)
            m_prev[i] = codes[i];
    }

private:
    bool WasDown(uint32_t code) const
    {
        for (int i = 0; i < 6; i++)
            if (m_prev[i] == code) return true;
        return false;
    }

    uint32_t m_prev[6] = {0, 0, 0, 0, 0, 0};
};
