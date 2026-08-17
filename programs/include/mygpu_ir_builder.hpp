#pragma once
#include <temrixstd.h>

enum
{
    MYIR_MAGIC = 0x4D594952u
};
enum
{
    IRT_F32 = 0,
    IRT_VEC2,
    IRT_VEC3,
    IRT_VEC4,
    IRT_MAT4
};
enum
{
    IRSTAGE_VERTEX = 0,
    IRSTAGE_FRAGMENT = 1
};
enum
{
    IROP_LOAD_INPUT = 1,
    IROP_LOAD_UNIFORM = 2,
    IROP_MUL = 3,
    IROP_ADD = 4,
    IROP_MOV = 5,
    IROP_SAMPLE_TEX = 6,
    IROP_OUTPUT = 7,
    IROP_SUB = 8,
    IROP_DIV = 9,
    IROP_DOT = 10,
    IROP_LENGTH = 11,
    IROP_ABS = 12,
    IROP_MAX = 13,
    IROP_MIN = 14,
    IROP_STEP = 15,
    IROP_SMOOTHSTEP = 16,
    IROP_MIX = 17,
    IROP_LOAD_CONST = 18,
    IROP_SWIZZLE = 19,
    IROP_CONSTRUCT_VEC4 = 20,
    IROP_LT = 21,
    IROP_LE = 22,
    IROP_GT = 23,
    IROP_GE = 24,
    IROP_EQ = 25,
    IROP_LOAD_VARYING = 26,
    IROP_END = 0xFF,
};
enum
{
    IRSWZ_XYZ = 0,
    IRSWZ_W = 1,
    IRSWZ_XY = 2,
    IRSWZ_X = 3,
    IRSWZ_Y = 4,
    IRSWZ_Z = 5
};
enum
{
    IRSEM_POSITION = 0,
    IRSEM_COLOR = 1,
    IRSEM_VARYING_BASE = 2
};

#pragma pack(push, 1)
struct IrHeaderW
{
    uint32_t magic, stage;
    uint32_t num_inputs, num_uniforms, num_samplers, num_varyings;
    uint32_t num_consts, num_regs, num_instrs;
};
struct IrInstrW
{
    uint8_t op, type;
    uint16_t dst, src_a, src_b, src_c;
};
#pragma pack(pop)

struct IrBuilder
{
    uint32_t stage;
    uint8_t inputTypes[8];
    uint32_t numInputs = 0;
    uint8_t uniformTypes[8] = {};
    uint32_t numUniforms = 0;
    uint8_t samplerTypes[4];
    uint32_t numSamplers = 0;
    uint8_t varyingTypes[8];
    uint32_t numVaryings = 0;
    float consts[32];
    uint32_t numConsts = 0;
    IrInstrW instrs[64];
    uint32_t numInstrs = 0;
    uint32_t nextReg = 0;

    explicit IrBuilder(uint32_t s) : stage(s) {}

    uint32_t addInput(uint8_t type)
    {
        inputTypes[numInputs] = type;
        return numInputs++;
    }
    uint32_t addUniform(uint8_t type, uint32_t slot)
    {
        uniformTypes[slot] = type;
        if (slot + 1 > numUniforms)
            numUniforms = slot + 1;
        return slot;
    }
    uint32_t addVarying(uint8_t type)
    {
        varyingTypes[numVaryings] = type;
        return numVaryings++;
    }
    uint32_t addConst(float v)
    {
        consts[numConsts] = v;
        return numConsts++;
    }

    /* Generic 1-3 operand instruction; returns the new dst register. */
    uint32_t emit(uint8_t op, uint8_t type, uint32_t a = 0, uint32_t b = 0, uint32_t c = 0)
    {
        uint32_t dst = nextReg++;
        instrs[numInstrs++] = IrInstrW{op, type, (uint16_t)dst,
                                       (uint16_t)a, (uint16_t)b, (uint16_t)c};
        return dst;
    }

    /* IR_OUTPUT has no dst; 'semantic' is IRSEM_POSITION / IRSEM_COLOR /
     * IRSEM_VARYING_BASE + varying_index. */
    void emitOutput(uint8_t valueType, uint32_t valueReg, uint32_t semantic)
    {
        instrs[numInstrs++] = IrInstrW{IROP_OUTPUT, valueType, 0,
                                       (uint16_t)valueReg, (uint16_t)semantic, 0};
    }

    void end() { instrs[numInstrs++] = IrInstrW{IROP_END, 0, 0, 0, 0, 0}; }

    uint32_t byteSize() const
    {
        return sizeof(IrHeaderW) + numInputs + numUniforms + numSamplers + numVaryings +
               numConsts * sizeof(float) + numInstrs * sizeof(IrInstrW);
    }

    /* Packs the blob into 'out' (must be >= byteSize() bytes). Returns size. */
    uint32_t serialize(uint8_t *out) const
    {
        IrHeaderW hdr{MYIR_MAGIC, stage, numInputs, numUniforms, numSamplers,
                      numVaryings, numConsts, nextReg, numInstrs};
        uint32_t off = 0;
        Memory::Copy(out + off, &hdr, sizeof(hdr));
        off += sizeof(hdr);
        Memory::Copy(out + off, inputTypes, numInputs);
        off += numInputs;
        Memory::Copy(out + off, uniformTypes, numUniforms);
        off += numUniforms;
        Memory::Copy(out + off, samplerTypes, numSamplers);
        off += numSamplers;
        Memory::Copy(out + off, varyingTypes, numVaryings);
        off += numVaryings;
        Memory::Copy(out + off, consts, numConsts * sizeof(float));
        off += numConsts * sizeof(float);
        Memory::Copy(out + off, instrs, numInstrs * sizeof(IrInstrW));
        off += numInstrs * sizeof(IrInstrW);
        return off;
    }
};