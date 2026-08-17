#pragma once
#include "common.hpp"

enum Capability : uint64_t
{
    CAP_NONE = 0,
    CAP_WRITE = 1ull << 0,
    CAP_PCI_MSIX = 1ull << 1,
    CAP_IRQ = 1ull << 2,
    CAP_ALLOC_VECTORS = 1ull << 3,
    CAP_GRANT = 1ull << 4,

    CAP_ALL_SIMPLE = CAP_WRITE | CAP_PCI_MSIX | CAP_IRQ | CAP_ALLOC_VECTORS | CAP_GRANT,
};

static inline bool hasCap(uint64_t taskCaps, uint64_t required)
{
    return (taskCaps & required) == required;
}

enum class DeviceGrantKind : uint8_t
{
    SpecificDevice = 0,
    ClassWildcard = 1,
    AnyDevice = 2,
};

static constexpr uint32_t MAX_DEVICE_GRANTS = 16;

struct DeviceGrant
{
    bool used = false;
    DeviceGrantKind kind = DeviceGrantKind::SpecificDevice;
    uint8_t classCode = 0;
    uint32_t deviceIndex = 0;
};