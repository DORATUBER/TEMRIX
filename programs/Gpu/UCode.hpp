#pragma once
#include "HardwareIp.hpp"
#include "AmdGpuCommon.hpp"
#include "UCodeNaming.hpp"
#include <temrixstd.h>

namespace AMD
{
    static inline uint8_t ip_version_maj(uint32_t ver) { return (ver >> 24) & 0xFF; }
    static inline uint8_t ip_version_min(uint32_t ver) { return (ver >> 16) & 0xFF; }
    static inline uint8_t ip_version_rev(uint32_t ver) { return (ver >>  8) & 0xFF; }

    static void ucode_ip_version_decode(
        uint32_t ip_versions[MAX_HWIP][HWIP_MAX_INSTANCE],
        unsigned long apu_flags,
        amd_asic_type asic_type,
        int block_type,
        char *ucode_prefix,
        int len)
    {
        const char *legacy = ucode_legacy_naming(ip_versions, apu_flags, asic_type, block_type);
        if (legacy)
        {
            int i = 0;
            while (legacy[i] && i < len - 1)
            {
                ucode_prefix[i] = legacy[i];
                i++;
            }
            ucode_prefix[i] = '\0';
            return;
        }

        const char *ip_name = nullptr;
        switch (block_type)
        {
        case GC_HWIP:    ip_name = "gc";   break;
        case SDMA0_HWIP: ip_name = "sdma"; break;
        case MP0_HWIP:   ip_name = "psp";  break;
        case MP1_HWIP:   ip_name = "smu";  break;
        case UVD_HWIP:   ip_name = "vcn";  break;
        case VPE_HWIP:   ip_name = "vpe";  break;
        case ISP_HWIP:   ip_name = "isp";  break;
        default:
            ucode_prefix[0] = '\0';
            return;
        }

        uint32_t version = ip_versions[block_type][0];
        uint8_t maj = ip_version_maj(version);
        uint8_t min = ip_version_min(version);
        uint8_t rev = ip_version_rev(version);

        
        
        int bi = 0;

        
        for (int i = 0; ip_name[i] && bi < len - 1; i++)
            ucode_prefix[bi++] = ip_name[i];

        
        auto append_u8 = [&](uint8_t val)
        {
            if (bi < len - 1) ucode_prefix[bi++] = '_';
            if (val >= 100 && bi < len - 1) ucode_prefix[bi++] = '0' + val / 100;
            if (val >=  10 && bi < len - 1) ucode_prefix[bi++] = '0' + (val / 10) % 10;
            if (bi < len - 1)               ucode_prefix[bi++] = '0' + val % 10;
        };

        append_u8(maj);
        append_u8(min);
        append_u8(rev);

        ucode_prefix[bi] = '\0';
    }

} 