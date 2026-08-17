#pragma once

#include <temrixstd.h>
#include "mygpu_ir_builder.hpp"

#pragma pack(push, 1)
struct CmdHeader
{
    uint32_t opcode, size;
};

enum
{
    OP_UPLOAD = 1,
    OP_BIND_RENDER_TARGET = 2,
    OP_BIND_VERTEX_BUFFER = 3,
    OP_CLEAR = 4,
    OP_DRAW_TRIANGLES = 5,
    OP_COPY_FROM_VRAM = 6,
    OP_FREE_HANDLE = 7,
    OP_BIND_TEXTURE = 8,
    OP_CREATE_SHADER_PROGRAM = 9,
    OP_BIND_SHADER_PROGRAM = 10,
    OP_SET_UNIFORM = 11,
};

enum
{
    OP_SET_VERTEX_LAYOUT = 12
};

enum
{
    ATTRIB_TYPE_FLOAT = 0,
    ATTRIB_TYPE_BYTE = 1,
    ATTRIB_TYPE_UNSIGNED_BYTE = 2,
    ATTRIB_TYPE_SHORT = 3,
    ATTRIB_TYPE_UNSIGNED_SHORT = 4,
    ATTRIB_TYPE_INT = 5,
    ATTRIB_TYPE_UNSIGNED_INT = 6,
};

enum
{
    GFX_FORMAT_RGBA8 = 0,
    GFX_FORMAT_RGB565 = 1
};
enum
{
    GFX_FILTER_NEAREST = 0,
    GFX_FILTER_LINEAR = 1
};
enum
{
    GFX_WRAP_CLAMP = 0,
    GFX_WRAP_REPEAT = 1
};

struct UploadCmd
{
    CmdHeader hdr;
    uint32_t handle, bar_offset, size;
};
struct BindTextureCmd
{
    CmdHeader hdr;
    uint32_t handle, width, height, settings;
};
struct BindRenderTargetCmd
{
    CmdHeader hdr;
    uint32_t handle, color_handle, settings;
};
struct BindVertexBufferCmd
{
    CmdHeader hdr;
    uint32_t handle, vertex_count;
};
struct ClearCmd
{
    CmdHeader hdr;
    uint32_t rgba;
};
struct DrawTrianglesCmd
{
    CmdHeader hdr;
};
struct CopyFromVramCmd
{
    CmdHeader hdr;
    uint32_t handle, bar_offset, size;
};
struct FreeHandleCmd
{
    CmdHeader hdr;
    uint32_t handle;
};
struct CreateShaderProgramCmd
{
    CmdHeader hdr;
    uint32_t handle, vs_bar_offset, vs_size, fs_bar_offset, fs_size;
};
struct BindShaderProgramCmd
{
    CmdHeader hdr;
    uint32_t handle;
};
struct SetUniformCmd
{
    CmdHeader hdr;
    uint32_t handle, slot, ir_type, bar_offset, size;
};

#define MYGPU_MAX_VERTEX_ATTRIBS 8

struct VertexAttribDesc
{
    uint32_t location, components, type, normalized, offset;
};

struct SetVertexLayoutCmd
{
    CmdHeader hdr;
    uint32_t stride;
    uint32_t attrib_count;
    VertexAttribDesc attribs[MYGPU_MAX_VERTEX_ATTRIBS];
};

struct Vertex3D
{
    float x, y, z;
};
#pragma pack(pop)

#define REG_WIDTH 0x00
#define REG_HEIGHT 0x04
#define REG_SCANOUT_ADDR 0x08
#define REG_RING_BASE 0x0C
#define REG_RING_SIZE 0x10
#define REG_RING_HEAD 0x14
#define REG_DOORBELL 0x18

static constexpr uint32_t makeTextureSettings(uint32_t format, uint32_t minFilter,
                                              uint32_t magFilter, uint32_t wrap)
{
    return (format & 0xF) | ((minFilter & 0x3) << 4) |
           ((magFilter & 0x3) << 6) | ((wrap & 0x3) << 8);
}

static constexpr uint32_t makeTargetSettings(bool depthEnabled)
{
    return depthEnabled ? 0x1 : 0x0;
}

constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 6.28318530717958647692f;
constexpr float HALF_PI = 1.57079632679489661923f;

static inline float fabsf(float x)
{
    return x < 0.0f ? -x : x;
}

float sinf(float x)
{
    while (x > PI)
        x -= TWO_PI;
    while (x < -PI)
        x += TWO_PI;

    const float B = 4.0f / PI;
    const float C = -4.0f / (PI * PI);

    float y = B * x + C * x * fabsf(x);

    const float P = 0.225f;
    y = P * (y * fabsf(y) - y) + y;

    return y;
}

static inline float cosf(float x)
{
    return sinf(x + HALF_PI);
}

struct Mat4
{
    float m[16];
};

static Mat4 MakePerspective(float fovYRadians, float aspect, float nearZ, float farZ, bool flipY)
{
    float f = cosf(fovYRadians * 0.5f) / sinf(fovYRadians * 0.5f); 
    Mat4 r{};
    r.m[0] = f / aspect;
    r.m[5] = flipY ? -f : f;
    r.m[10] = (farZ + nearZ) / (nearZ - farZ);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
    return r;
}

static Mat4 MakeRotationY(float angleRadians)
{
    float c = cosf(angleRadians);
    float s = sinf(angleRadians);
    Mat4 r{};
    r.m[0] = c;
    r.m[8] = s;
    r.m[5] = 1.0f;
    r.m[2] = -s;
    r.m[10] = c;
    r.m[15] = 1.0f;
    return r;
}

static Mat4 MakeRotationX(float angleRadians)
{
    float c = cosf(angleRadians);
    float s = sinf(angleRadians);
    Mat4 r{};
    r.m[0] = 1.0f;
    r.m[5] = c;
    r.m[9] = -s;
    r.m[6] = s;
    r.m[10] = c;
    r.m[15] = 1.0f;
    return r;
}

static Mat4 MakeIdentity()
{
    Mat4 r{};
    r.m[0] = 1.0f;
    r.m[5] = 1.0f;
    r.m[10] = 1.0f;
    r.m[15] = 1.0f;
    return r;
}

static Mat4 MakeTranslation(float x, float y, float z)
{
    Mat4 r{};
    r.m[0] = 1.0f;
    r.m[5] = 1.0f;
    r.m[10] = 1.0f;
    r.m[15] = 1.0f;
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

static Mat4 MatMul(const Mat4 &a, const Mat4 &b)
{
    Mat4 r{};
    for (int col = 0; col < 4; col++)
    {
        for (int row = 0; row < 4; row++)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++)
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

class GpuDevice
{
public:
    bool Initialize()
    {
        uint64_t count = Syscall::Pci::Count();
        if (count == 0)
        {
            String::Print("[gpu] no PCI devices\n");
            return false;
        }

        Syscall::Pci::KernelDevice devices[64];
        uint64_t fetched = Syscall::Pci::GetDevices(devices, count);

        int64_t idx = -1;
        for (uint64_t i = 0; i < fetched; i++)
        {
            if (devices[i].vendorId == 0xCAFE && devices[i].deviceId == 0x0001)
            {
                idx = (int64_t)i;
                break;
            }
        }
        if (idx < 0)
        {
            String::Print("[gpu] device not found (vendor=0xcafe dev=0x0001)\n");
            return false;
        }
        else
        {
            String::Print("[gpu] Device found (vendor=0xcafe dev=0x0001)\n");
        }

        m_regsVirt = Syscall::Memory::MapBar((uint64_t)idx, 0);
        m_vramVirt = Syscall::Memory::MapBar((uint64_t)idx, 1);
        if (!m_regsVirt || !m_vramVirt)
        {
            String::Print("[gpu] failed to map BARs\n");
            return false;
        }
        else
        {
            String::Print("[gpu] Succeded to map BARs\n");
        }

        m_regs = (volatile uint32_t *)m_regsVirt;
        m_vram = (volatile uint8_t *)m_vramVirt;

        FramebufferInfo fbInfo{};
        if (Syscall::Info::Get(InfoFramebuffer, &fbInfo) != 0)
        {
            String::Print("[gpu] failed to query framebuffer info\n");
            return false;
        }
        else
        {
            String::Print("[gpu] Succeded to query framebuffer info\n");
        }
        m_width = fbInfo.width;
        m_height = fbInfo.height;

        m_regs[REG_WIDTH / 4] = m_width;
        m_regs[REG_HEIGHT / 4] = m_height;

        m_frameSize = m_width * m_height * 4;

        m_ringOffset = m_frameSize * 2;
        m_ringSize = 4096;

        m_regs[REG_RING_BASE / 4] = m_ringOffset;
        m_regs[REG_RING_SIZE / 4] = m_ringSize;

        m_ringTail = 0;

        return true;
    }

    void CreateTestPerspective(uint32_t vboHandle, uint32_t texHandle,
                               uint32_t rtHandle, uint32_t shaderHandle)
    {
        m_vbo = vboHandle;
        m_tex = texHandle;
        m_rt = rtHandle;
        m_shader = shaderHandle;

        IrBuilder vs(IRSTAGE_VERTEX);
        uint32_t vsInPos = vs.addInput(IRT_VEC3);
        uint32_t uniMvp = vs.addUniform(IRT_MAT4, 0);
        {
            uint32_t rPos3 = vs.emit(IROP_LOAD_INPUT, IRT_VEC3, vsInPos);
            uint32_t cOne = vs.addConst(1.0f);
            uint32_t rOne = vs.emit(IROP_LOAD_CONST, IRT_F32, cOne);
            uint32_t rPos4 = vs.emit(IROP_CONSTRUCT_VEC4, IRT_VEC4, rPos3, rOne);
            uint32_t rMvp = vs.emit(IROP_LOAD_UNIFORM, IRT_MAT4, uniMvp);
            uint32_t rClip = vs.emit(IROP_MUL, IRT_VEC4, rMvp, rPos4);
            vs.emitOutput(IRT_VEC4, rClip, IRSEM_POSITION);
            vs.end();
        }

        IrBuilder fs(IRSTAGE_FRAGMENT);
        fs.addUniform(IRT_MAT4, 0); 
        uint32_t uniColor = fs.addUniform(IRT_VEC3, 1);
        {
            uint32_t rColor = fs.emit(IROP_LOAD_UNIFORM, IRT_VEC3, uniColor);
            uint32_t cOne = fs.addConst(1.0f);
            uint32_t rOne = fs.emit(IROP_LOAD_CONST, IRT_F32, cOne);
            uint32_t rOut = fs.emit(IROP_CONSTRUCT_VEC4, IRT_VEC4, rColor, rOne);
            fs.emitOutput(IRT_VEC4, rOut, IRSEM_COLOR);
            fs.end();
        }

        uint8_t vsBuf[512], fsBuf[512];
        uint32_t vsSize = vs.serialize(vsBuf);
        uint32_t fsSize = fs.serialize(fsBuf);

        const float h = 0.5f;
        Vertex3D cube[36] = {
            
            {-h, -h, h},
            {h, -h, h},
            {h, h, h},
            {-h, -h, h},
            {h, h, h},
            {-h, h, h},
            
            {h, -h, -h},
            {-h, -h, -h},
            {-h, h, -h},
            {h, -h, -h},
            {-h, h, -h},
            {h, h, -h},
            
            {-h, -h, -h},
            {-h, -h, h},
            {-h, h, h},
            {-h, -h, -h},
            {-h, h, h},
            {-h, h, -h},
            
            {h, -h, h},
            {h, -h, -h},
            {h, h, -h},
            {h, -h, h},
            {h, h, -h},
            {h, h, h},
            
            {-h, h, h},
            {h, h, h},
            {h, h, -h},
            {-h, h, h},
            {h, h, -h},
            {-h, h, -h},
            
            {-h, -h, -h},
            {h, -h, -h},
            {h, -h, h},
            {-h, -h, -h},
            {h, -h, h},
            {-h, -h, h},
        };
        const uint32_t triDataSize = sizeof(cube);

        const uint32_t vertexOffset = m_ringOffset + m_ringSize;
        const uint32_t vsBlobOffset = vertexOffset + triDataSize;
        const uint32_t fsBlobOffset = vsBlobOffset + vsSize;
        const uint32_t mvpOffset = fsBlobOffset + fsSize;
        m_color0Offset = mvpOffset + sizeof(Mat4);

        Memory::Copy((void *)(m_vram + vertexOffset), cube, triDataSize);
        Memory::Copy((void *)(m_vram + vsBlobOffset), vsBuf, vsSize);
        Memory::Copy((void *)(m_vram + fsBlobOffset), fsBuf, fsSize);

        float aspect = (float)m_width / (float)m_height;
        m_proj = MakePerspective(60.0f * PI / 180.0f, aspect, 0.1f, 100.0f, /*flipY=*/true);
        m_view = MakeIdentity(); 

        Mat4 model = MatMul(MakeTranslation(0.0f, 0.0f, -3.0f),
                            MatMul(MakeRotationY(0.0f), MakeRotationX(0.0f)));
        Mat4 mvp = MatMul(m_proj, MatMul(m_view, model));
        Memory::Copy((void *)(m_vram + mvpOffset), mvp.m, sizeof(Mat4));

        float color0[3] = {1.0f, 0.3f, 0.2f};
        Memory::Copy((void *)(m_vram + m_color0Offset), color0, sizeof(color0));

        m_uniMvp = uniMvp;
        m_mvpOffset = mvpOffset;
        m_angle = 0.0f;

        Push(UploadCmd{{OP_UPLOAD, sizeof(UploadCmd)}, m_vbo, vertexOffset, triDataSize});
        Push(BindTextureCmd{{OP_BIND_TEXTURE, sizeof(BindTextureCmd)}, m_tex, m_width, m_height, makeTextureSettings(GFX_FORMAT_RGBA8, GFX_FILTER_NEAREST, GFX_FILTER_NEAREST, GFX_WRAP_CLAMP)});
        Push(BindRenderTargetCmd{{OP_BIND_RENDER_TARGET, sizeof(BindRenderTargetCmd)},
                                 m_rt,
                                 m_tex,
                                 makeTargetSettings(true)});
        Push(CreateShaderProgramCmd{{OP_CREATE_SHADER_PROGRAM, sizeof(CreateShaderProgramCmd)},
                                    m_shader,
                                    vsBlobOffset,
                                    vsSize,
                                    fsBlobOffset,
                                    fsSize});
        Push(BindShaderProgramCmd{{OP_BIND_SHADER_PROGRAM, sizeof(BindShaderProgramCmd)}, m_shader});

        SetVertexLayoutCmd layout{};
        layout.hdr = {OP_SET_VERTEX_LAYOUT, sizeof(SetVertexLayoutCmd)};
        layout.stride = sizeof(Vertex3D);
        layout.attrib_count = 1;
        layout.attribs[0] = VertexAttribDesc{0, 3, ATTRIB_TYPE_FLOAT, 0, 0};
        Push(layout);

        Push(SetUniformCmd{{OP_SET_UNIFORM, sizeof(SetUniformCmd)},
                           m_shader,
                           m_uniMvp,
                           IRT_MAT4,
                           mvpOffset,
                           sizeof(Mat4)});
        Push(SetUniformCmd{{OP_SET_UNIFORM, sizeof(SetUniformCmd)},
                           m_shader,
                           uniColor,
                           IRT_VEC3,
                           m_color0Offset,
                           sizeof(color0)});

        m_regs[REG_DOORBELL / 4] = m_ringTail;
    }

    void DrawTestPerspective()
    {
        m_angle += 0.02f;
        if (m_angle > TWO_PI)
            m_angle -= TWO_PI;

        Mat4 model = MatMul(MakeTranslation(0.0f, 0.0f, -3.0f),
                            MatMul(MakeRotationY(m_angle), MakeRotationX(m_angle * 0.7f)));
        Mat4 mvp = MatMul(m_proj, MatMul(m_view, model));
        Memory::Copy((void *)(m_vram + m_mvpOffset), mvp.m, sizeof(Mat4));

        Push(SetUniformCmd{{OP_SET_UNIFORM, sizeof(SetUniformCmd)},
                           m_shader,
                           m_uniMvp,
                           IRT_MAT4,
                           m_mvpOffset,
                           sizeof(Mat4)});

        Push(ClearCmd{{OP_CLEAR, sizeof(ClearCmd)}, 0x101018FF});
        Push(BindVertexBufferCmd{{OP_BIND_VERTEX_BUFFER, sizeof(BindVertexBufferCmd)}, m_vbo, 36});
        Push(DrawTrianglesCmd{{OP_DRAW_TRIANGLES, sizeof(DrawTrianglesCmd)}});
        Push(CopyFromVramCmd{{OP_COPY_FROM_VRAM, sizeof(CopyFromVramCmd)},
                             m_rt,
                             m_useFb1 ? m_frameSize : 0,
                             m_frameSize});
        m_regs[REG_DOORBELL / 4] = m_ringTail;
        m_regs[REG_SCANOUT_ADDR / 4] = m_useFb1 ? m_frameSize : 0;
    }

private:
    void RingWrite(uint32_t ringOffset, uint32_t ringSize, uint32_t pos,
                   const void *data, uint32_t len)
    {
        uint32_t firstPart = (len < ringSize - pos) ? len : (ringSize - pos);
        Memory::Copy((void *)(m_vram + ringOffset + pos), data, firstPart);
        if (firstPart < len)
            Memory::Copy((void *)(m_vram + ringOffset), (const uint8_t *)data + firstPart, len - firstPart);
    }

    template <typename T>
    void Push(const T &pkt)
    {
        uint32_t size = sizeof(T);
        RingWrite(m_ringOffset, m_ringSize, m_ringTail, &pkt, size);
        m_ringTail = (m_ringTail + size) % m_ringSize;
    }

    uint64_t m_regsVirt = 0, m_vramVirt = 0;
    volatile uint32_t *m_regs = nullptr;
    volatile uint8_t *m_vram = nullptr;

    uint32_t m_width = 0, m_height = 0, m_frameSize = 0;
    uint32_t m_ringOffset = 0, m_ringSize = 0, m_ringTail = 0;
    bool m_useFb1 = false;

    uint32_t m_vbo = 0, m_tex = 0, m_rt = 0, m_shader = 0;
    uint32_t m_uniHalfSize = 0, m_uniRadius = 0, m_uniColor0 = 0;
    uint32_t m_color0Offset = 0;

    uint32_t m_radiusOffset = 0;
    float m_baseRadius = 0.0f;

    
    uint32_t m_uniMvp = 0;
    uint32_t m_mvpOffset = 0;
    float m_angle = 0.0f;
    Mat4 m_proj{};
    Mat4 m_view{};
};