#pragma once
#include <temrixstd.h>
#include "CompositorClient.hpp"
#include "Graphics.hpp"

struct WindowTheme
{
    uint32_t titlebarBg = 0xFF13131F;
    uint32_t accentLine = 0xFF6C6FCC;
    uint32_t divider = 0xFF0D0D18;
    uint32_t titleColor = 0xFFBFBFCF;
    uint32_t btnClose = 0xFFFF5F57;
    uint32_t btnMin = 0xFFFFBD2E;
    uint32_t btnMax = 0xFF28C840;
};

struct WindowOptions
{
    bool decorated = true;
    bool fullscreen = false;
    bool background = false;
};

struct WindowInputEvent
{
    uint8_t  type;   
    uint32_t modifierKeys;
    uint32_t keyCodes[6];
    int32_t  localX, localY;
    int32_t  deltaX, deltaY;
    uint32_t buttons;
    bool     inTitlebar;
};

class Window
{
public:
    static constexpr uint32_t TITLEBAR_H = 36;
    static constexpr uint32_t ACCENT_H = 2;
    static constexpr int BTN_RADIUS = 6;
    static constexpr int BTN_Y = (int)TITLEBAR_H / 2;
    static constexpr int BTN_START_X = 14;
    static constexpr int BTN_SPACING = 20;

    bool Init(int32_t x, int32_t y, uint32_t width, uint32_t height,
              const char *title, const WindowTheme &theme = {},
              const WindowOptions &opts = {})
    {
        m_opts = opts;
        m_contentW = width;
        m_contentH = height;
        m_titlebarH = opts.decorated ? TITLEBAR_H : 0;
        m_totalH = height + m_titlebarH;
        m_theme = theme;

        int i = 0;
        while (title[i] && i < 63)
        {
            m_title[i] = title[i];
            i++;
        }
        m_title[i] = '\0';

        return m_client.init(x, y, width, m_totalH,
                              opts.background ? LayerBackground : LayerNormal);
    }

    uint32_t *ContentBuffer() { return m_client.pixels() + m_titlebarH * m_contentW; }
    uint32_t ContentWidth() const { return m_contentW; }
    uint32_t ContentHeight() const { return m_contentH; }

    void BeginFrame() { m_client.beginFrame(); }

    void Present()
    {
        if (m_opts.decorated)
            DrawDecorations(m_client.pixels());
        m_client.presentAll();
    }

    void PresentRect(int32_t x, int32_t y, uint32_t w, uint32_t h)
    {
        if (m_opts.decorated)
            DrawDecorations(m_client.pixels());
        m_client.present(x, (int32_t)(y + m_titlebarH), w, h);
    }

    void Move(int32_t x, int32_t y) { m_client.move(x, y); }
    void Shutdown() { m_client.shutdown(); }

    template <typename Fn>
    void PollInput(Fn &&callback)
    {
        SharedFrameBufferSlot *slot = m_client.Slot();
        if (!slot)
            return;

        uint32_t w = __atomic_load_n(&slot->inputWriteIndex, __ATOMIC_ACQUIRE);
        uint32_t r = __atomic_load_n(&slot->inputReadIndex, __ATOMIC_RELAXED);

        while (r != w)
        {
            const SharedInputEvent &raw = slot->inputEvents[r & (MAX_INPUT_EVENTS - 1)];

            WindowInputEvent ev{};
            ev.type = raw.type;
            ev.modifierKeys = raw.modifierKeys;
            for (int k = 0; k < 6; k++)
                ev.keyCodes[k] = raw.keyCodes[k];
            ev.deltaX = raw.deltaX;
            ev.deltaY = raw.deltaY;
            ev.buttons = raw.buttons;
            ev.localX = raw.localX;
            ev.localY = raw.localY - (int32_t)m_titlebarH;
            ev.inTitlebar = m_opts.decorated && raw.localY < (int32_t)m_titlebarH;

            callback(ev);
            r++;
        }

        __atomic_store_n(&slot->inputReadIndex, r, __ATOMIC_RELEASE);
    }

    bool IsFocused()
    {
        SharedFrameBufferSlot *slot = m_client.Slot();
        return slot && slot->focused;
    }

private:
    CompositorClient m_client;
    WindowOptions m_opts;
    uint32_t m_contentW = 0;
    uint32_t m_contentH = 0;
    uint32_t m_titlebarH = 0;
    uint32_t m_totalH = 0;
    WindowTheme m_theme;
    char m_title[64];

    void DrawDecorations(uint32_t *buf)
    {
        uint32_t w = m_contentW;
        uint32_t totalH = m_totalH;

        for (uint32_t x = 0; x < w; x++)
            buf[0 * w + x] = m_theme.accentLine;
        for (uint32_t x = 0; x < w; x++)
            buf[1 * w + x] = m_theme.accentLine;

        for (uint32_t row = ACCENT_H; row < TITLEBAR_H - 1; row++)
            for (uint32_t x = 0; x < w; x++)
                buf[row * w + x] = m_theme.titlebarBg;

        for (uint32_t x = 0; x < w; x++)
            buf[(TITLEBAR_H - 1) * w + x] = m_theme.divider;

        DrawCircle(buf, w, totalH, BTN_START_X + 0 * BTN_SPACING, BTN_Y, BTN_RADIUS, m_theme.btnClose);
        DrawCircle(buf, w, totalH, BTN_START_X + 1 * BTN_SPACING, BTN_Y, BTN_RADIUS, m_theme.btnMin);
        DrawCircle(buf, w, totalH, BTN_START_X + 2 * BTN_SPACING, BTN_Y, BTN_RADIUS, m_theme.btnMax);

        int titleLen = 0;
        while (m_title[titleLen])
            titleLen++;
        int textX = ((int)w - titleLen * 8) / 2;
        int textY = ((int)TITLEBAR_H - 16) / 2;
        Graphics::DrawString(buf, textX, textY, m_title,
                             m_theme.titleColor, 1,
                             (int)w, (int)totalH, w);
    }

    void DrawCircle(uint32_t *buf, uint32_t w, uint32_t h,
                    int cx, int cy, int r, uint32_t color)
    {
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++)
            {
                if (dx * dx + dy * dy > r * r)
                    continue;
                int px = cx + dx, py = cy + dy;
                if (px < 0 || py < 0 || px >= (int)w || py >= (int)h)
                    continue;
                buf[py * w + px] = color;
            }
    }
};