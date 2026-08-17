#pragma once
#include "common.hpp"

static constexpr uint32_t MAX_SHARED_GRANTS = 16;

struct SharedGrant {
    bool     used;
    uint32_t handle; 
    bool     canWrite;
    uint32_t grantedBy;
};