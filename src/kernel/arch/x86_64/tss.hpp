#pragma once
#include "common.hpp"

namespace Hardware
{
    namespace TSS
    {
        constexpr uint8_t IST_CRITICAL = 1;
        constexpr uint8_t IST_NMI = 2;
        constexpr uint8_t MAX_CORES = 8;

        struct __attribute__((packed)) Entry
        {
            uint32_t reserved0;
            uint64_t rsp[3];
            uint64_t reserved1;
            uint64_t ist[7];
            uint8_t reserved2[10];
            uint16_t iomap_base;
        };

        void init();
        void initAP(int core);
        void setRsp0(uint64_t rsp);
    }
}