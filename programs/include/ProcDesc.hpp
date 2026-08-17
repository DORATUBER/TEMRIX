#pragma once
#include <temrixstd/stdint.h>

constexpr uint32_t PROC_DESC_MAX = 16;

enum class DescKind : uint32_t
{
    Empty         = 0,
    SharedConsole = 1,
};

struct ProcDescriptor
{
    DescKind kind;
    uint32_t flags;
    uint64_t handle;
    uint64_t param;
};

struct ProcDescTable
{
    uint32_t count;
    uint32_t reserved;
    ProcDescriptor descriptors[PROC_DESC_MAX];
};

static_assert(sizeof(ProcDescTable) <= 0x1000, "ProcDescTable must fit in one page");