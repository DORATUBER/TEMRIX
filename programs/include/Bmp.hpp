#pragma once
#include <temrixstd.h>
#include "FileSystem/FsClient.hpp"

struct BmpFileHeader
{
    uint16_t signature;
    uint32_t fileSize;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t pixelOffset;
} __attribute__((packed));

struct BmpDibHeader
{
    uint32_t dibSize;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bitsPerPixel;
    uint32_t compression;
    uint32_t imageSize;
    int32_t xPixelsPerMeter;
    int32_t yPixelsPerMeter;
    uint32_t colorsUsed;
    uint32_t colorsImportant;
} __attribute__((packed));

inline bool LoadBmp(const char *path, FsClient &client,
                    uint32_t **outBuf, uint32_t *outW, uint32_t *outH)
{
    uint64_t virt = 0;
    uint32_t len = 0;

    if (!client.readFile(path, &virt, &len))
    {
        String::Print("[bmp] not found: ");
        String::Print(path);
        String::Print("\n");
        return false;
    }

    const uint8_t *raw = reinterpret_cast<const uint8_t *>(virt);

    if (len < sizeof(BmpFileHeader) + sizeof(BmpDibHeader))
    {
        String::Print("[bmp] file too small\n");
        return false;
    }

    const BmpFileHeader *fhdr = reinterpret_cast<const BmpFileHeader *>(raw);
    if (fhdr->signature != 0x4D42)
    {
        String::Print("[bmp] not a BMP file\n");
        return false;
    }

    const BmpDibHeader *dib = reinterpret_cast<const BmpDibHeader *>(raw + sizeof(BmpFileHeader));

    if (dib->planes != 1)
    {
        String::Print("[bmp] unsupported plane count\n");
        return false;
    }
    if (dib->compression != 0 && dib->compression != 3)
    {
        String::Print("[bmp] unsupported compression\n");
        return false;
    }
    if (dib->bitsPerPixel != 24 && dib->bitsPerPixel != 32)
    {
        String::Print("[bmp] only 24bpp and 32bpp supported\n");
        return false;
    }

    int32_t bmpW = dib->width;
    int32_t bmpH = dib->height;
    bool topDown = (bmpH < 0);
    uint32_t imgW = (uint32_t)bmpW;
    uint32_t imgH = topDown ? (uint32_t)(-bmpH) : (uint32_t)bmpH;

    if (imgW == 0 || imgH == 0)
    {
        String::Print("[bmp] zero dimensions\n");
        return false;
    }

    uint32_t bytesPerPixel = dib->bitsPerPixel / 8;
    uint32_t rowStride = (imgW * bytesPerPixel + 3) & ~3u;

    if (fhdr->pixelOffset + rowStride * imgH > len)
    {
        String::Print("[bmp] pixel data out of bounds\n");
        return false;
    }

    const uint8_t *pixelData = raw + fhdr->pixelOffset;

    uint64_t size = (uint64_t)imgW * imgH * 4;
    uint32_t *buf = (uint32_t *)Syscall::Memory::Map(size);
    if (!buf)
    {
        String::Print("[bmp] out of memory\n");
        return false;
    }

    for (uint32_t row = 0; row < imgH; row++)
    {
        uint32_t srcRow = topDown ? row : (imgH - 1 - row);
        const uint8_t *src = pixelData + srcRow * rowStride;
        uint32_t *dst = buf + row * imgW;

        if (bytesPerPixel == 3)
        {
            for (uint32_t col = 0; col < imgW; col++, src += 3)
            {
                uint8_t b = src[0], g = src[1], r = src[2];
                dst[col] = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            }
        }
        else
        {
            for (uint32_t col = 0; col < imgW; col++, src += 4)
            {
                uint8_t b = src[0], g = src[1], r = src[2], a = src[3];
                if (a == 0)
                    a = 0xFF;
                dst[col] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            }
        }
    }

    *outBuf = buf;
    *outW = imgW;
    *outH = imgH;
    String::Print("[bmp] loaded\n");
    return true;
}