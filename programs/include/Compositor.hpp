#pragma once
#include <temrixstd.h>
#include "SharedFramebuffer.hpp"
#include "Graphics.hpp"
#include "InputClient.hpp"

struct SlotState
{
    bool attached;
    uint32_t *pixels[2];
    int32_t x, y;
    uint32_t width, height;
    uint32_t lastDirtyFrame;
};

struct CPoint
{
    int x, y;
};

static bool CursorPointInPolygon(int px, int py, const CPoint *pts, int count)
{
    bool inside = false;
    for (int i = 0, j = count - 1; i < count; j = i++)
    {
        int xi = pts[i].x, yi = pts[i].y;
        int xj = pts[j].x, yj = pts[j].y;
        bool crosses = ((yi > py) != (yj > py)) &&
                       (px < (xj - xi) * (py - yi) / (yj - yi) + xi);
        if (crosses)
            inside = !inside;
    }
    return inside;
}

static const CPoint kCursorOutline[] = {
    {0, 0}, {0, 14}, {3, 11}, {5, 16}, {7, 15}, {5, 10}, {9, 10}};
static constexpr int kCursorOutlinePointCount =
    sizeof(kCursorOutline) / sizeof(kCursorOutline[0]);
static constexpr int kCursorBoundW = 12;
static constexpr int kCursorBoundH = 18;

class Compositor
{
public:
    Compositor()
        : front(nullptr), back(nullptr), sw(0), sh(0), pps(0), reg(nullptr),
          haveInput(false), cursorX(0), cursorY(0), prevCursorX(0), prevCursorY(0),
          leftButtonWasDown(false), activeSlot(-1), backgroundSlot(-1), firstFrame(true)
    {
        for (int i = 0; i < MAX_SHARED_FB; i++)
        {
            slots[i] = {false, {nullptr, nullptr}, 0, 0, 0, 0, 0};
            zOrder[i] = -1;
        }
    }

    bool Initialize()
    {
        FramebufferInfo fbInfo{};
        if (Syscall::Info::Get(InfoFramebuffer, &fbInfo) != 0)
            return false;

        sw = fbInfo.width;
        sh = fbInfo.height;
        pps = fbInfo.pixelsPerScanLine;

        front = MapFramebuffer(fbInfo.physAddr);
        back = (uint32_t *)Syscall::Memory::Map((uint64_t)pps * sh * sizeof(uint32_t));

        cursorX = (int)(sw / 2);
        cursorY = (int)(sh / 2);
        prevCursorX = cursorX;
        prevCursorY = cursorY;

        haveInput = input.init();
        if (haveInput)
            input.RequestFocus();
        else
            String::Print("[compositor] input server not available, cursor disabled\n");

        return (front && back && SetupRegistry());
    }

    void Run()
    {
        while (true)
        {
            PollInput();
            UpdateSlots();
            Render();
            Syscall::Process::Wait();
        }
    }

private:
    uint32_t *front, *back;
    uint32_t sw, sh, pps;
    SlotState slots[MAX_SHARED_FB];
    SharedFBRegistry *reg;

    InputClient input;
    bool haveInput;
    int cursorX, cursorY;
    int prevCursorX, prevCursorY;
    bool leftButtonWasDown;

    int32_t zOrder[MAX_SHARED_FB];
    int32_t activeSlot;
    int32_t backgroundSlot;
    bool firstFrame;

    uint32_t *MapFramebuffer(uint64_t fbPhys)
    {
        Syscall::Pci::KernelDevice devices[64];
        uint64_t count = Syscall::Pci::GetDevices(devices, 64);

        for (uint64_t i = 0; i < count; i++)
        {
            if (devices[i].classCode != 0x03)
                continue;
            for (int b = 0; b < 6; b++)
            {
                if (devices[i].barSizes[b] == 0)
                    continue;
                if (fbPhys >= devices[i].bars[b] && fbPhys < devices[i].bars[b] + devices[i].barSizes[b])
                {
                    uint64_t virt = Syscall::Memory::MapBar(i, b);
                    if (!virt)
                        continue;
                    return (uint32_t *)(virt + (fbPhys - devices[i].bars[b]));
                }
            }
        }
        return nullptr;
    }

    bool SetupRegistry()
    {
        uint64_t pages = (sizeof(SharedFBRegistry) + 0xFFF) >> 12;

        Syscall::Memory::SharedMemResult result;
        if (Syscall::Memory::CreateShared(
                pages,
                ::Memory::Read | ::Memory::Write | ::Memory::User,
                &result) != 0)
            return false;

        reg = (SharedFBRegistry *)result.virtAddr;
        for (uint64_t i = 0; i < sizeof(SharedFBRegistry); i++)
            ((uint8_t *)reg)[i] = 0;

        uint64_t handle = result.handle;
        Syscall::Service::Publish("compositor", &handle, sizeof(handle));

        uint32_t myPid = Syscall::Process::GetId();
        char pidStr[21];
        Syscall::Service::Publish("compositor.pid", String::FromU64(myPid, pidStr));

        return true;
    }

    void RemoveFromZOrder(int slotIndex)
    {
        int write = 0;
        for (int read = 0; read < MAX_SHARED_FB; read++)
        {
            if (zOrder[read] == slotIndex)
                continue;
            zOrder[write++] = zOrder[read];
        }
        for (; write < MAX_SHARED_FB; write++)
            zOrder[write] = -1;
    }

    void RaiseToFront(int slotIndex)
    {
        if (slotIndex == backgroundSlot)
            return;

        RemoveFromZOrder(slotIndex);
        int write = 0;
        int32_t tmp[MAX_SHARED_FB];
        for (int i = 0; i < MAX_SHARED_FB; i++)
            tmp[i] = -1;
        for (int i = 0; i < MAX_SHARED_FB; i++)
            if (zOrder[i] != -1)
                tmp[write++] = zOrder[i];
        if (write < MAX_SHARED_FB)
            tmp[write++] = slotIndex;
        for (int i = 0; i < MAX_SHARED_FB; i++)
            zOrder[i] = tmp[i];
    }

    int HitTest(int px, int py)
    {
        for (int i = MAX_SHARED_FB - 1; i >= 0; i--)
        {
            int idx = zOrder[i];
            if (idx < 0 || !slots[idx].attached)
                continue;
            if (PointInRect(px, py, idx))
                return idx;
        }
        if (backgroundSlot >= 0 && slots[backgroundSlot].attached &&
            PointInRect(px, py, backgroundSlot))
            return backgroundSlot;
        return -1;
    }

    bool PointInRect(int px, int py, int slotIndex)
    {
        return px >= slots[slotIndex].x && px < slots[slotIndex].x + (int)slots[slotIndex].width &&
               py >= slots[slotIndex].y && py < slots[slotIndex].y + (int)slots[slotIndex].height;
    }

    void PushEvent(int slotIndex, const SharedInputEvent &ev)
    {
        if (slotIndex < 0 || !slots[slotIndex].attached)
            return;

        SharedFrameBufferSlot *slot = &reg->slots[slotIndex];
        uint32_t w = __atomic_load_n(&slot->inputWriteIndex, __ATOMIC_RELAXED);
        uint32_t r = __atomic_load_n(&slot->inputReadIndex, __ATOMIC_ACQUIRE);

        if (w - r >= MAX_INPUT_EVENTS)
            return;

        slot->inputEvents[w & (MAX_INPUT_EVENTS - 1)] = ev;
        __atomic_store_n(&slot->inputWriteIndex, w + 1, __ATOMIC_RELEASE);
    }

    void SetFocus(int newActive)
    {
        if (newActive == activeSlot)
            return;

        if (activeSlot >= 0 && slots[activeSlot].attached)
        {
            reg->slots[activeSlot].focused = 0;
            SharedInputEvent ev{};
            ev.type = InputEventFocusLost;
            PushEvent(activeSlot, ev);
        }

        activeSlot = newActive;

        if (activeSlot >= 0)
        {
            reg->slots[activeSlot].focused = 1;
            RaiseToFront(activeSlot);
            SharedInputEvent ev{};
            ev.type = InputEventFocusGained;
            PushEvent(activeSlot, ev);
        }
    }

    void PollInput()
    {
        if (!haveInput)
            return;

        input.Poll([&](const InputEvent &event)
                   {
            if (event.type == InputEventTypeMouse)
            {
                cursorX += event.mouseDeltaX;
                cursorY += event.mouseDeltaY;
                if (cursorX < 0) cursorX = 0;
                if (cursorY < 0) cursorY = 0;
                if (cursorX >= (int)sw) cursorX = (int)sw - 1;
                if (cursorY >= (int)sh) cursorY = (int)sh - 1;

                bool leftDown = (event.mouseButtons & 0x1) != 0;
                bool leftClick = leftDown && !leftButtonWasDown;
                leftButtonWasDown = leftDown;

                if (leftClick)
                {
                    int hit = HitTest(cursorX, cursorY);
                    SetFocus(hit);
                }

                if (activeSlot >= 0)
                {
                    SharedInputEvent ev{};
                    ev.type = InputEventMouse;
                    ev.localX = cursorX - slots[activeSlot].x;
                    ev.localY = cursorY - slots[activeSlot].y;
                    ev.deltaX = event.mouseDeltaX;
                    ev.deltaY = event.mouseDeltaY;
                    ev.buttons = event.mouseButtons;
                    PushEvent(activeSlot, ev);
                }
            }
            else if (event.type == InputEventTypeKeyboard)
            {
                if (activeSlot >= 0)
                {
                    SharedInputEvent ev{};
                    ev.type = InputEventKeyboard;
                    ev.modifierKeys = event.modifierKeys;
                    for (int i = 0; i < 6; i++)
                        ev.keyCodes[i] = event.keyCodes[i];
                    PushEvent(activeSlot, ev);
                }
            } });
    }

    void UpdateSlots()
    {
        for (int i = 0; i < MAX_SHARED_FB; i++)
        {
            SharedFrameBufferSlot *slot = &reg->slots[i];

            if (slots[i].attached && !slot->ready)
            {
                slots[i].attached = false;
                slots[i].pixels[0] = nullptr;
                slots[i].pixels[1] = nullptr;
                if (i == backgroundSlot)
                    backgroundSlot = -1;
                else
                    RemoveFromZOrder(i);
                if (activeSlot == i)
                    SetFocus(-1);
                continue;
            }

            if (!slot->ready || slots[i].attached || !slot->shmem_id[0] || !slot->shmem_id[1])
                continue;

            Syscall::Memory::SharedMemResult result0;
            if (Syscall::Memory::MapShared(slot->shmem_id[0], ::Memory::Read | ::Memory::User, &result0) != 0)
                continue;

            Syscall::Memory::SharedMemResult result1;
            if (Syscall::Memory::MapShared(slot->shmem_id[1], ::Memory::Read | ::Memory::User, &result1) != 0)
            {
                Syscall::Memory::UnmapShared(result0.virtAddr, result0.pageCount);
                continue;
            }

            slots[i] = {
                true,
                {(uint32_t *)result0.virtAddr, (uint32_t *)result1.virtAddr},
                slot->x,
                slot->y,
                slot->width,
                slot->height,
                0,
            };

            if (slot->layer == LayerBackground)
            {
                if (backgroundSlot == -1)
                {
                    backgroundSlot = i;
                }
                else
                {
                    String::Print("[compositor] warning: multiple background windows, demoting extra to normal\n");
                    for (int z = 0; z < MAX_SHARED_FB; z++)
                    {
                        if (zOrder[z] == -1)
                        {
                            zOrder[z] = i;
                            break;
                        }
                    }
                }
                continue;
            }

            for (int z = 0; z < MAX_SHARED_FB; z++)
            {
                if (zOrder[z] == -1)
                {
                    zOrder[z] = i;
                    break;
                }
            }
        }
    }

    static void PlotBlendStrided(uint32_t *buf, uint32_t bufW, uint32_t bufH, uint32_t stride,
                                 int x, int y, uint32_t color)
    {
        if (x < 0 || y < 0 || x >= (int)bufW || y >= (int)bufH)
            return;
        buf[y * stride + x] = Graphics::Blend(color, buf[y * stride + x]);
    }

    static void DrawCursor(uint32_t *buf, uint32_t bufW, uint32_t bufH, uint32_t stride,
                           int cursorX, int cursorY)
    {
        for (int row = -1; row <= kCursorBoundH; row++)
        {
            for (int col = -1; col <= kCursorBoundW; col++)
            {
                bool filled = CursorPointInPolygon(col, row, kCursorOutline, kCursorOutlinePointCount);
                if (filled)
                {
                    PlotBlendStrided(buf, bufW, bufH, stride, cursorX + col, cursorY + row, 0xFFFFFFFF);
                    continue;
                }

                bool touchesFill = false;
                for (int oy = -1; oy <= 1 && !touchesFill; oy++)
                    for (int ox = -1; ox <= 1 && !touchesFill; ox++)
                        if (CursorPointInPolygon(col + ox, row + oy, kCursorOutline, kCursorOutlinePointCount))
                            touchesFill = true;

                if (touchesFill)
                    PlotBlendStrided(buf, bufW, bufH, stride, cursorX + col, cursorY + row, 0xFF000000);
            }
        }
    }

    struct FrameSnap
    {
        bool dirty;
        uint32_t frame, which;
        int32_t dx, dy, dw, dh;
    };

    void RenderSlot(int i, const FrameSnap &snap, int32_t unionX0, int32_t unionY0,
                    int32_t unionX1, int32_t unionY1)
    {
        uint32_t *srcPixels = slots[i].pixels[snap.which];
        if (!srcPixels)
            return;

        int32_t sx0 = slots[i].x, sy0 = slots[i].y;
        int32_t clipX0 = sx0 > unionX0 ? sx0 : unionX0;
        int32_t clipY0 = sy0 > unionY0 ? sy0 : unionY0;
        int32_t clipX1 = (sx0 + (int32_t)slots[i].width) < unionX1 ? (sx0 + (int32_t)slots[i].width) : unionX1;
        int32_t clipY1 = (sy0 + (int32_t)slots[i].height) < unionY1 ? (sy0 + (int32_t)slots[i].height) : unionY1;

        for (int32_t dy = clipY0; dy < clipY1; dy++)
        {
            uint32_t row = (uint32_t)(dy - sy0);
            for (int32_t dx = clipX0; dx < clipX1; dx++)
            {
                uint32_t col = (uint32_t)(dx - sx0);
                uint32_t src = srcPixels[row * slots[i].width + col];
                if ((src >> 24) == 0)
                    continue;
                back[dy * pps + dx] = Graphics::Blend(src, back[dy * pps + dx]);
            }
        }
    }

    void Render()
    {
        FrameSnap snap[MAX_SHARED_FB] = {};

        int32_t unionX0 = INT32_MAX, unionY0 = INT32_MAX;
        int32_t unionX1 = INT32_MIN, unionY1 = INT32_MIN;
        bool anyDirty = false;

        for (int i = 0; i < MAX_SHARED_FB; i++)
        {
            if (!slots[i].attached)
                continue;
            SharedFrameBufferSlot *slot = &reg->slots[i];

            uint32_t current = __atomic_load_n(&slot->dirtyFrame, __ATOMIC_ACQUIRE);
            snap[i].frame = current;
            snap[i].which = __atomic_load_n(&slot->presentIndex, __ATOMIC_RELAXED) & 1;
            snap[i].dx = slot->dirtyX;
            snap[i].dy = slot->dirtyY;
            snap[i].dw = slot->dirtyW;
            snap[i].dh = slot->dirtyH;
            snap[i].dirty = (current != slots[i].lastDirtyFrame);

            if (!snap[i].dirty)
                continue;
            anyDirty = true;

            int32_t rx = slots[i].x + snap[i].dx;
            int32_t ry = slots[i].y + snap[i].dy;
            int32_t rw = snap[i].dw ? snap[i].dw : slots[i].width;
            int32_t rh = snap[i].dh ? snap[i].dh : slots[i].height;
            if (rx < unionX0)
                unionX0 = rx;
            if (ry < unionY0)
                unionY0 = ry;
            if (rx + rw > unionX1)
                unionX1 = rx + rw;
            if (ry + rh > unionY1)
                unionY1 = ry + rh;
        }

        bool cursorDirty = firstFrame || (cursorX != prevCursorX || cursorY != prevCursorY);
        if (cursorDirty)
        {
            int32_t oldX0 = prevCursorX - 2, oldY0 = prevCursorY - 2;
            int32_t oldX1 = prevCursorX + kCursorBoundW + 2, oldY1 = prevCursorY + kCursorBoundH + 2;
            int32_t newX0 = cursorX - 2, newY0 = cursorY - 2;
            int32_t newX1 = cursorX + kCursorBoundW + 2, newY1 = cursorY + kCursorBoundH + 2;

            if (oldX0 < unionX0)
                unionX0 = oldX0;
            if (oldY0 < unionY0)
                unionY0 = oldY0;
            if (oldX1 > unionX1)
                unionX1 = oldX1;
            if (oldY1 > unionY1)
                unionY1 = oldY1;
            if (newX0 < unionX0)
                unionX0 = newX0;
            if (newY0 < unionY0)
                unionY0 = newY0;
            if (newX1 > unionX1)
                unionX1 = newX1;
            if (newY1 > unionY1)
                unionY1 = newY1;
        }

        if (!anyDirty && !cursorDirty)
            return;

        if (unionX0 < 0)
            unionX0 = 0;
        if (unionY0 < 0)
            unionY0 = 0;
        if (unionX1 > (int32_t)sw)
            unionX1 = sw;
        if (unionY1 > (int32_t)sh)
            unionY1 = sh;

        if (backgroundSlot >= 0 && slots[backgroundSlot].attached)
            RenderSlot(backgroundSlot, snap[backgroundSlot], unionX0, unionY0, unionX1, unionY1);

        for (int z = 0; z < MAX_SHARED_FB; z++)
        {
            int i = zOrder[z];
            if (i < 0 || !slots[i].attached)
                continue;
            RenderSlot(i, snap[i], unionX0, unionY0, unionX1, unionY1);
        }

        DrawCursor(back, sw, sh, pps, cursorX, cursorY);

        SwapBuffersRect(unionX0, unionY0, unionX1 - unionX0, unionY1 - unionY0);

        for (int i = 0; i < MAX_SHARED_FB; i++)
        {
            if (!slots[i].attached || !snap[i].dirty)
                continue;
            SharedFrameBufferSlot *slot = &reg->slots[i];
            slots[i].lastDirtyFrame = snap[i].frame;
            __atomic_store_n(&slot->consumedFrame, snap[i].frame, __ATOMIC_RELEASE);
        }

        prevCursorX = cursorX;
        prevCursorY = cursorY;
        firstFrame = false;
    }

    void SwapBuffersRect(int32_t x, int32_t y, uint32_t w, uint32_t h)
    {
        for (uint32_t row = 0; row < h; row++)
        {
            uint32_t *srcRow = &back[(y + row) * pps + x];
            uint32_t *dstRow = &front[(y + row) * pps + x];
            for (uint32_t col = 0; col < w; col++)
                dstRow[col] = srcRow[col];
        }
    }
};