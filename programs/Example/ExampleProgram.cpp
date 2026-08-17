#include <temrixstd.h>
#include <temrixstd/stdlib.h>
#include "Window.hpp"
#include "FileSystem/FsClient.hpp"
#include "Bmp.hpp"
#include "Graphics.hpp"
#include "InputClient.hpp"
#include "temrixstd/trx_so.h"

#define NDEBUG
#define STBI_NO_STDIO
#define STBI_NO_THREAD_LOCALS
#define STBI_NO_FAILURE_STRINGS
#define STBI_NO_SIMD

#define STBI_MALLOC(sz) malloc(sz)
#define STBI_REALLOC(p, sz) realloc(p, sz)
#define STBI_FREE(p) free(p)

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.hpp"

static const uint32_t kMaxInitW = 1280;
static const uint32_t kMaxInitH = 800;
static const uint32_t kMinInitW = 200;
static const uint32_t kMinInitH = 150;
static const uint32_t kFrameGap = 100; 

static void PrintU64(const char *label, uint64_t value)
{
    char buf[21];
    String::Print(label);
    String::Print(String::FromU64(value, buf));
    String::Print("\n");
}

static void PrintUsage(const char *argv0)
{
    String::Print("Usage: ");
    String::Print(argv0 ? argv0 : "ImageViewer");
    String::Print(" <path-to-png>\n");
}

static void DrawImage(uint32_t *pixels, uint32_t windowW, uint32_t windowH,
                      const unsigned char *image, int imageW, int imageH)
{
    uint32_t drawW = windowW;
    uint32_t drawH = (uint32_t)(((uint64_t)imageH * drawW) / imageW);

    if (drawH > windowH)
    {
        drawH = windowH;
        drawW = (uint32_t)(((uint64_t)imageW * drawH) / imageH);
    }

    drawW = drawW ? drawW : 1;
    drawH = drawH ? drawH : 1;

    uint32_t offsetX = (windowW - drawW) / 2;
    uint32_t offsetY = (windowH - drawH) / 2;

    
    for (uint32_t y = 0; y < drawH; y++)
    {
        uint32_t srcY = ((uint64_t)y * imageH) / drawH;

        for (uint32_t x = 0; x < drawW; x++)
        {
            uint32_t srcX = ((uint64_t)x * imageW) / drawW;
            const unsigned char *src = image + ((uint64_t)srcY * imageW + srcX) * 4;

            uint32_t color = ((uint32_t)src[3] << 24) | ((uint32_t)src[0] << 16) |
                             ((uint32_t)src[1] << 8) | (uint32_t)src[2];

            pixels[(offsetY + y) * windowW + (offsetX + x)] = color;
        }
    }

    
    for (uint32_t x = 0; x < drawW; x++)
    {
        Graphics::PutPixel(pixels, offsetX + x, offsetY, 0xFFFFFFFF, windowW, windowH, windowW);
        Graphics::PutPixel(pixels, offsetX + x, offsetY + drawH - 1, 0xFFFFFFFF, windowW, windowH, windowW);
    }

    for (uint32_t y = 0; y < drawH; y++)
    {
        Graphics::PutPixel(pixels, offsetX, offsetY + y, 0xFFFFFFFF, windowW, windowH, windowW);
        Graphics::PutPixel(pixels, offsetX + drawW - 1, offsetY + y, 0xFFFFFFFF, windowW, windowH, windowW);
    }
}

int main(int argc, char **argv)
{
    String::Print("[viewer] starting image viewer\n");

    if (argc < 2 || !argv[1] || !argv[1][0])
    {
        String::Print("[viewer] no image path provided\n");
        PrintUsage(argc > 0 ? argv[0] : nullptr);
        return -1;
    }

    const char *path = argv[1];
    String::Print("[viewer] image path = ");
    String::Print(path);
    String::Print("\n");

    FsClient fsClient;
    if (!fsClient.init())
    {
        String::Print("[viewer] FsClient init FAILED\n");
        return -1;
    }

    uint64_t virt = 0;
    uint32_t fileSize = 0;

    if (!fsClient.readFile(path, &virt, &fileSize))
    {
        String::Print("[viewer] readFile FAILED\n");
        return -1;
    }

    PrintU64("[viewer] file size = ", fileSize);
    const uint8_t *fileData = reinterpret_cast<const uint8_t *>(virt);

    int imageW = 0, imageH = 0, imageChannels = 0;

    if (!stbi_info_from_memory(fileData, (int)fileSize, &imageW, &imageH, &imageChannels))
    {
        String::Print("[viewer] stbi_info_from_memory FAILED (unsupported or corrupt file)\n");
        return -1;
    }

    PrintU64("[viewer] width = ", (uint64_t)imageW);
    PrintU64("[viewer] height = ", (uint64_t)imageH);
    PrintU64("[viewer] channels = ", (uint64_t)imageChannels);

    int decodedW = 0, decodedH = 0, decodedChannels = 0;

    unsigned char *image = stbi_load_from_memory(
        fileData, (int)fileSize, &decodedW, &decodedH, &decodedChannels, 4 /* force RGBA */);

    if (!image)
    {
        String::Print("[viewer] stbi_load_from_memory FAILED\n");
        return -1;
    }

    String::Print("[viewer] decode OK\n");

    uint32_t initW = (uint32_t)decodedW / 2;
    uint32_t initH = (uint32_t)decodedH / 2;

    if (initW > kMaxInitW || initH > kMaxInitH)
    {
        initW = kMaxInitW;
        initH = kMaxInitH;
    }
    if (initW < kMinInitW)
        initW = kMinInitW;
    if (initH < kMinInitH)
        initH = kMinInitH;

    WindowOptions opts;
    opts.decorated = true;
    opts.fullscreen = false;

    Window window;
    if (!window.Init(100, 100, initW, initH, path, {}, opts))
    {
        String::Print("[viewer] Window::Init FAILED\n");
        stbi_image_free(image);
        return -1;
    }

    window.BeginFrame();

    uint32_t *pixels = window.ContentBuffer();
    uint32_t windowW = window.ContentWidth();
    uint32_t windowH = window.ContentHeight();

    Graphics::DrawRect(pixels, 0, 0, (int)windowW, (int)windowH,
                       0xFF202020, (int)windowW, (int)windowH, windowW);

    DrawImage(pixels, windowW, windowH, image, decodedW, decodedH);

    window.Present();

    stbi_image_free(image);
    return 0;
}