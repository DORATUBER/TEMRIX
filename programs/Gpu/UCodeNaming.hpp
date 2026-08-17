#pragma once
#include "HardwareIp.hpp"
#include "AmdGpuCommon.hpp"
#include "AsicTypes.hpp"
#include <temrixstd.h>

namespace AMD
{
    static inline uint32_t ip_version_masked(uint32_t ver)
    {
        return ver & ~0xFFu;
    }

    static const char* ucode_legacy_naming(
        uint32_t ip_versions[MAX_HWIP][HWIP_MAX_INSTANCE],
        unsigned long apu_flags,
        amd_asic_type asic_type,
        int block_type)
    {
        auto ipver = [&](int hw_ip, int inst = 0) -> uint32_t {
            return ip_version_masked(ip_versions[hw_ip][inst]);
        };

        if (block_type == MP0_HWIP)
        {
            switch (ipver(MP0_HWIP))
            {
            case IP_VERSION(9, 0, 0):
                switch (asic_type)
                {
                case CHIP_VEGA10: return "vega10";
                case CHIP_VEGA12: return "vega12";
                default:          return nullptr;
                }
            case IP_VERSION(10, 0, 0):
            case IP_VERSION(10, 0, 1):
                if (asic_type == CHIP_RAVEN)
                {
                    if (apu_flags & AMD_APU_IS_RAVEN2)   return "raven2";
                    if (apu_flags & AMD_APU_IS_PICASSO)   return "picasso";
                    return "raven";
                }
                break;
            case IP_VERSION(11, 0, 0):  return "navi10";
            case IP_VERSION(11, 0, 2):  return "vega20";
            case IP_VERSION(11, 0, 3):  return "renoir";
            case IP_VERSION(11, 0, 4):  return "arcturus";
            case IP_VERSION(11, 0, 5):  return "navi14";
            case IP_VERSION(11, 0, 7):  return "sienna_cichlid";
            case IP_VERSION(11, 0, 9):  return "navi12";
            case IP_VERSION(11, 0, 11): return "navy_flounder";
            case IP_VERSION(11, 0, 12): return "dimgrey_cavefish";
            case IP_VERSION(11, 0, 13): return "beige_goby";
            case IP_VERSION(11, 5, 0):
            case IP_VERSION(11, 5, 2):  return "vangogh";
            case IP_VERSION(12, 0, 1):  return "green_sardine"; 
            case IP_VERSION(13, 0, 2):  return "aldebaran";
            case IP_VERSION(13, 0, 1):
            case IP_VERSION(13, 0, 3):  return "yellow_carp";
            }
        }
        else if (block_type == MP1_HWIP)
        {
            switch (ipver(MP1_HWIP))
            {
            case IP_VERSION(9, 0, 0):
            case IP_VERSION(10, 0, 0):
            case IP_VERSION(10, 0, 1):
            case IP_VERSION(11, 0, 2):
                if (asic_type == CHIP_ARCTURUS) return "arcturus_smc";
                return nullptr;
            case IP_VERSION(11, 0, 0):  return "navi10_smc";
            case IP_VERSION(11, 0, 5):  return "navi14_smc";
            case IP_VERSION(11, 0, 9):  return "navi12_smc";
            case IP_VERSION(11, 0, 7):  return "sienna_cichlid_smc";
            case IP_VERSION(11, 0, 11): return "navy_flounder_smc";
            case IP_VERSION(11, 0, 12): return "dimgrey_cavefish_smc";
            case IP_VERSION(11, 0, 13): return "beige_goby_smc";
            case IP_VERSION(13, 0, 2):  return "aldebaran_smc";
            }
        }
        else if (block_type == SDMA0_HWIP)
        {
            switch (ipver(SDMA0_HWIP))
            {
            case IP_VERSION(4, 0, 0): return "vega10_sdma";
            case IP_VERSION(4, 0, 1): return "vega12_sdma";
            case IP_VERSION(4, 1, 0):
            case IP_VERSION(4, 1, 1):
                if (apu_flags & AMD_APU_IS_RAVEN2)   return "raven2_sdma";
                if (apu_flags & AMD_APU_IS_PICASSO)   return "picasso_sdma";
                return "raven_sdma";
            case IP_VERSION(4, 1, 2):
                if (apu_flags & AMD_APU_IS_RENOIR) return "renoir_sdma";
                return "green_sardine_sdma"; 
            case IP_VERSION(4, 2, 0):  return "vega20_sdma";
            case IP_VERSION(4, 2, 2):  return "arcturus_sdma";
            case IP_VERSION(4, 4, 0):  return "aldebaran_sdma";
            case IP_VERSION(5, 0, 0):  return "navi10_sdma";
            case IP_VERSION(5, 0, 1):  return "cyan_skillfish2_sdma";
            case IP_VERSION(5, 0, 2):  return "navi14_sdma";
            case IP_VERSION(5, 0, 5):  return "navi12_sdma";
            case IP_VERSION(5, 2, 0):  return "sienna_cichlid_sdma";
            case IP_VERSION(5, 2, 2):  return "navy_flounder_sdma";
            case IP_VERSION(5, 2, 4):  return "dimgrey_cavefish_sdma";
            case IP_VERSION(5, 2, 5):  return "beige_goby_sdma";
            case IP_VERSION(5, 2, 3):  return "yellow_carp_sdma";
            case IP_VERSION(5, 2, 1):  return "vangogh_sdma";
            }
        }
        else if (block_type == UVD_HWIP)
        {
            switch (ipver(UVD_HWIP))
            {
            case IP_VERSION(1, 0, 0):
            case IP_VERSION(1, 0, 1):
                if (apu_flags & AMD_APU_IS_RAVEN2)   return "raven2_vcn";
                if (apu_flags & AMD_APU_IS_PICASSO)   return "picasso_vcn";
                return "raven_vcn";
            case IP_VERSION(2, 5, 0): return "arcturus_vcn";
            case IP_VERSION(2, 2, 0):
                if (apu_flags & AMD_APU_IS_RENOIR) return "renoir_vcn";
                return "green_sardine_vcn"; 
            case IP_VERSION(2, 6, 0): return "aldebaran_vcn";
            case IP_VERSION(2, 0, 0): return "navi10_vcn";
            case IP_VERSION(2, 0, 2):
                if (asic_type == CHIP_NAVI12) return "navi12_vcn";
                return "navi14_vcn";
            case IP_VERSION(3, 0, 0):
            case IP_VERSION(3, 0, 64):
            case IP_VERSION(3, 0, 192):
                if (ipver(GC_HWIP) == IP_VERSION(10, 3, 0))
                    return "sienna_cichlid_vcn";
                return "navy_flounder_vcn";
            case IP_VERSION(3, 0, 2):  return "vangogh_vcn";
            case IP_VERSION(3, 0, 16): return "dimgrey_cavefish_vcn";
            case IP_VERSION(3, 0, 33): return "beige_goby_vcn";
            case IP_VERSION(3, 1, 1):  return "yellow_carp_vcn";
            }
        }
        else if (block_type == GC_HWIP)
        {
            switch (ipver(GC_HWIP))
            {
            case IP_VERSION(9, 0, 1): return "vega10";
            case IP_VERSION(9, 2, 1): return "vega12";
            case IP_VERSION(9, 4, 0): return "vega20";
            case IP_VERSION(9, 2, 2):
            case IP_VERSION(9, 1, 0):
                if (apu_flags & AMD_APU_IS_RAVEN2)   return "raven2";
                if (apu_flags & AMD_APU_IS_PICASSO)   return "picasso";
                return "raven";
            case IP_VERSION(9, 4, 1): return "arcturus";
            case IP_VERSION(9, 3, 0):
                if (apu_flags & AMD_APU_IS_RENOIR) return "renoir";
                return "green_sardine"; 
            case IP_VERSION(9, 4, 2):   return "aldebaran";
            case IP_VERSION(10, 1, 10): return "navi10";
            case IP_VERSION(10, 1, 1):  return "navi14";
            case IP_VERSION(10, 1, 2):  return "navi12";
            case IP_VERSION(10, 3, 0):  return "sienna_cichlid";
            case IP_VERSION(10, 3, 2):  return "navy_flounder";
            case IP_VERSION(10, 3, 1):  return "vangogh";
            case IP_VERSION(10, 3, 4):  return "dimgrey_cavefish";
            case IP_VERSION(10, 3, 5):  return "beige_goby";
            case IP_VERSION(10, 3, 3):  return "yellow_carp";
            case IP_VERSION(10, 1, 3):
            case IP_VERSION(10, 1, 4):  return "cyan_skillfish2";
            }
        }

        return nullptr;
    }

} 