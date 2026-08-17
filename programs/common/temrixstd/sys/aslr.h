#pragma once

#include "loader_types.h"
#include <temrixstd/entropy.h>
#include <temrixstd/stdio.h>

static inline bool pickLoadBias(uint64_t imageSpan, const LoaderConfig &cfg, uint64_t *outBias)
{
    if (imageSpan + cfg.guardGap < imageSpan ||
        cfg.baseMin + imageSpan + cfg.guardGap > cfg.baseMax)
    {
        if (cfg.debug)
            String::Printf("[loader] imageSpan=0x%llx does not fit in window\n", imageSpan);
        return false;
    }

    uint64_t usableRange = cfg.baseMax - cfg.baseMin - imageSpan - cfg.guardGap;
    uint64_t numSlots = usableRange / cfg.alignment;
    if (numSlots == 0)
        return false;

    uint64_t slot = getRandom64() % numSlots;
    *outBias = cfg.baseMin + slot * cfg.alignment;

    if (cfg.debug)
        String::Printf("[loader] chosen bias=0x%llx (slot %llu/%llu)\n", *outBias, slot, numSlots);

    return true;
}