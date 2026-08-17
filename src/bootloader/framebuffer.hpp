#pragma once

#include "efi.hpp"

struct Framebuffer
{
    EfiGraphicsOutputProtocol *gop;

    uint64_t base;
    uint32_t width, height, pitch;
    bool valid;

    Framebuffer() : gop(nullptr), base(0), width(0), height(0), pitch(0), valid(false) {}

    EfiStatus init(EfiBootServices *bs)
    {
        uint64_t count = 0;
        EfiHandle *handles = nullptr;

        EfiStatus s = bs->LocateHandleBuffer(2, &GopGuid, nullptr, &count, &handles);
        if (s != EfiSuccess) return s;

        s = bs->HandleProtocol(handles[0], &GopGuid, (void **)&gop);
        if (s != EfiSuccess) return s;

        refresh();
        valid = true;
        return EfiSuccess;
    }

    
    EfiStatus setMode(uint32_t modeIndex)
    {
        EfiStatus s = gop->SetMode(gop, modeIndex);
        if (s != EfiSuccess) return s;

        refresh();
        return EfiSuccess;
    }

private:
    void refresh()
    {
        EfiGraphicsOutputProtocolMode *mode = gop->Mode;
        EfiGraphicsOutputModeInformation *modeInfo = mode->Info;

        base   = mode->FrameBufferBase;
        width  = modeInfo->HorizontalResolution;
        height = modeInfo->VerticalResolution;
        pitch  = modeInfo->PixelsPerScanLine;
    }
};