#pragma once
#include "GpuMmio.hpp"
#include "discovery.hpp"
#include "HardwareIp.hpp"
#include <temrixstd/string.h>
#include "sdma.hpp"
#include "vpe.hpp"
#include "AmdGpuCommon.hpp"
#include "AsicTypes.hpp"
#include "UCode.hpp"

namespace AMD
{
    static constexpr uint16_t HW_ID_MAX = 300;

    struct MemRange
    {
        uint64_t base_address;
        uint64_t limit_address;
    };

    class IpDiscovery;

    enum AmdIpBlockType
    {
        AMD_IP_BLOCK_TYPE_COMMON,
        AMD_IP_BLOCK_TYPE_GMC,
        AMD_IP_BLOCK_TYPE_IH,
        AMD_IP_BLOCK_TYPE_PSP,
        AMD_IP_BLOCK_TYPE_SMC,
        AMD_IP_BLOCK_TYPE_DCE,
        AMD_IP_BLOCK_TYPE_GFX,
        AMD_IP_BLOCK_TYPE_SDMA,
        AMD_IP_BLOCK_TYPE_UVD,
        AMD_IP_BLOCK_TYPE_VCE,
        AMD_IP_BLOCK_TYPE_VCN,
        AMD_IP_BLOCK_TYPE_JPEG,
        AMD_IP_BLOCK_TYPE_MES,
        AMD_IP_BLOCK_TYPE_VPE,
        AMD_IP_BLOCK_TYPE_ISP,
        AMD_IP_BLOCK_TYPE_UMSCH_MM,
        AMD_IP_BLOCK_TYPE_COUNT
    };

    static const char *hw_id_name(uint16_t hw_id)
    {
        switch (hw_id)
        {
        case MP1_HWID:
            return "MP1";
        case MP2_HWID:
            return "MP2";
        case THM_HWID:
            return "THM";
        case SMUIO_HWID:
            return "SMUIO";
        case FUSE_HWID:
            return "FUSE";
        case CLKA_HWID:
            return "CLKA";
        case PWR_HWID:
            return "PWR";
        case GC_HWID:
            return "GC";
        case UVD_HWID:
            return "UVD";
        case AUDIO_AZ_HWID:
            return "AUDIO_AZ";
        case ACP_HWID:
            return "ACP";
        case DCI_HWID:
            return "DCI";
        case DMU_HWID:
            return "DMU";
        case DCO_HWID:
            return "DCO";
        case DIO_HWID:
            return "DIO";
        case XDMA_HWID:
            return "XDMA";
        case DCEAZ_HWID:
            return "DCEAZ";
        case DAZ_HWID:
            return "DAZ";
        case SDPMUX_HWID:
            return "SDPMUX";
        case NTB_HWID:
            return "NTB";
        case IOHC_HWID:
            return "IOHC";
        case L2IMU_HWID:
            return "L2IMU";
        case VCE_HWID:
            return "VCE";
        case MMHUB_HWID:
            return "MMHUB";
        case ATHUB_HWID:
            return "ATHUB";
        case DBGU_NBIO_HWID:
            return "DBGU_NBIO";
        case DFX_HWID:
            return "DFX";
        case DBGU0_HWID:
            return "DBGU0";
        case DBGU1_HWID:
            return "DBGU1";
        case OSSSYS_HWID:
            return "OSSSYS";
        case HDP_HWID:
            return "HDP";
        case SDMA0_HWID:
            return "SDMA0";
        case SDMA1_HWID:
            return "SDMA1";
        case SDMA2_HWID:
            return "SDMA2";
        case SDMA3_HWID:
            return "SDMA3";
        case LSDMA_HWID:
            return "LSDMA";
        case ISP_HWID:
            return "ISP";
        case DBGU_IO_HWID:
            return "DBGU_IO";
        case DF_HWID:
            return "DF";
        case CLKB_HWID:
            return "CLKB";
        case FCH_HWID:
            return "FCH";
        case DFX_DAP_HWID:
            return "DFX_DAP";
        case L1IMU_PCIE_HWID:
            return "L1IMU_PCIE";
        case L1IMU_NBIF_HWID:
            return "L1IMU_NBIF";
        case L1IMU_IOAGR_HWID:
            return "L1IMU_IOAGR";
        case L1IMU3_HWID:
            return "L1IMU3";
        case L1IMU4_HWID:
            return "L1IMU4";
        case L1IMU5_HWID:
            return "L1IMU5";
        case L1IMU6_HWID:
            return "L1IMU6";
        case L1IMU7_HWID:
            return "L1IMU7";
        case L1IMU8_HWID:
            return "L1IMU8";
        case L1IMU9_HWID:
            return "L1IMU9";
        case L1IMU10_HWID:
            return "L1IMU10";
        case L1IMU11_HWID:
            return "L1IMU11";
        case L1IMU12_HWID:
            return "L1IMU12";
        case L1IMU13_HWID:
            return "L1IMU13";
        case L1IMU14_HWID:
            return "L1IMU14";
        case L1IMU15_HWID:
            return "L1IMU15";
        case WAFLC_HWID:
            return "WAFLC";
        case FCH_USB_PD_HWID:
            return "FCH_USB_PD";
        case PCIE_HWID:
            return "PCIE";
        case PCS_HWID:
            return "PCS";
        case DDCL_HWID:
            return "DDCL";
        case SST_HWID:
            return "SST";
        case IOAGR_HWID:
            return "IOAGR";
        case NBIF_HWID:
            return "NBIF";
        case IOAPIC_HWID:
            return "IOAPIC";
        case SYSTEMHUB_HWID:
            return "SYSTEMHUB";
        case NTBCCP_HWID:
            return "NTBCCP";
        case UMC_HWID:
            return "UMC";
        case SATA_HWID:
            return "SATA";
        case USB_HWID:
            return "USB";
        case CCXSEC_HWID:
            return "CCXSEC";
        case XGMI_HWID:
            return "XGMI";
        case XGBE_HWID:
            return "XGBE";
        case MP0_HWID:
            return "MP0";
        case VPE_HWID:
            return "VPE";
        default:
            return "UNKNOWN";
        }
    }

    static const int *get_hw_id_map()
    {
        static int map[MAX_HWIP] = {}; 
        static bool initialized = false;
        if (!initialized)
        {
            map[GC_HWIP] = GC_HWID;
            map[HDP_HWIP] = HDP_HWID;
            map[SDMA0_HWIP] = SDMA0_HWID;
            map[SDMA1_HWIP] = SDMA1_HWID;
            map[SDMA2_HWIP] = SDMA2_HWID;
            map[SDMA3_HWIP] = SDMA3_HWID;
            map[LSDMA_HWIP] = LSDMA_HWID;
            map[MMHUB_HWIP] = MMHUB_HWID;
            map[ATHUB_HWIP] = ATHUB_HWID;
            map[NBIO_HWIP] = NBIF_HWID;
            map[MP0_HWIP] = MP0_HWID;
            map[MP1_HWIP] = MP1_HWID;
            map[UVD_HWIP] = UVD_HWID;
            map[VCE_HWIP] = VCE_HWID;
            map[DF_HWIP] = DF_HWID;
            map[DCE_HWIP] = DMU_HWID;
            map[OSSSYS_HWIP] = OSSSYS_HWID;
            map[SMUIO_HWIP] = SMUIO_HWID;
            map[PWR_HWIP] = PWR_HWID;
            map[NBIF_HWIP] = NBIF_HWID;
            map[THM_HWIP] = THM_HWID;
            map[CLK_HWIP] = CLKA_HWID;
            map[UMC_HWIP] = UMC_HWID;
            map[XGMI_HWIP] = XGMI_HWID;
            map[DCI_HWIP] = DCI_HWID;
            map[PCIE_HWIP] = PCIE_HWID;
            map[VPE_HWIP] = VPE_HWID;
            map[ISP_HWIP] = ISP_HWID;
            initialized = true;
        }
        return map;
    }

    struct HarvestInfo
    {
        uint32_t gc_active_mask; 
        uint32_t sdma_active_mask;
        uint32_t vcn_inst_mask;       
        uint32_t vcn_harvest_config;  
        uint32_t jpeg_inst_mask;      
        uint32_t jpeg_harvest_config; 
        uint64_t umc_active_mask;
        uint32_t harvest_ip_mask; 
    };

    struct IpBlockFuncs
    {
        bool (*early_init)(IpDiscovery *);
        bool (*hw_init)(IpDiscovery *);
        bool (*hw_fini)(IpDiscovery *);
    };

    struct IpBlock
    {
        AmdIpBlockType type;
        uint8_t major;
        uint8_t minor;
        uint8_t rev;
        const char *name;
        const IpBlockFuncs *funcs; 
        bool initialized;
    };

    enum GpuFamily
    {
        AMDGPU_FAMILY_UNKNOWN = 0,
        AMDGPU_FAMILY_AI,  
        AMDGPU_FAMILY_RV,  
        AMDGPU_FAMILY_NV,  
        AMDGPU_FAMILY_VGH, 
        AMDGPU_FAMILY_YC,  
        AMDGPU_FAMILY_GC_10_3_6,
        AMDGPU_FAMILY_GC_10_3_7,
        AMDGPU_FAMILY_GC_11_0_0,
        AMDGPU_FAMILY_GC_11_0_1,
        AMDGPU_FAMILY_GC_11_5_0,
        AMDGPU_FAMILY_GC_12_0_0,
    };

    class IpDiscovery
    {
    public:
        bool init(GpuMemory &mem,  const Syscall::Pci::KernelDevice &gpu)
        {
            m_mem = &mem;
            m_gpu = gpu;
            m_buffer = (uint8_t *)Syscall::Memory::Map(DISCOVERY_TMR_SIZE);

            Memory::Set(m_ip_versions, 0,sizeof(uint32_t)*MAX_HWIP*HWIP_MAX_INSTANCE);
            Memory::Set(m_reg_offset, 0,sizeof(uint32_t)*MAX_HWIP*HWIP_MAX_INSTANCE);
            
            if (!read_binary())
                return false;
            if (!verify())
                return false;
            if (!parse())
                return false;
            if (!get_gfx_info())
                return false;
            if (!get_mall_info())
                return false;
            if (!get_vcn_info())
                return false;
            if (!get_nps_info())
                return false;
            m_harvest = get_harvest_info(m_gpu);

            if (!set_ip_blocks())
                return false;

            String::Printf("MMHUB: 0x%x\n", m_ip_versions[MMHUB_HWIP][0]);
            String::Printf("ATHUB: 0x%x\n", m_ip_versions[ATHUB_HWIP][0]);
            String::Printf("NBIO:  0x%x\n", m_ip_versions[NBIO_HWIP][0]);
            String::Printf("HDP:   0x%x\n", m_ip_versions[HDP_HWIP][0]);

            return true;
        }

        uint32_t ip_version(uint8_t hw_ip, uint8_t inst = 0) const
        {
            if (hw_ip >= MAX_HWIP || inst >= HWIP_MAX_INSTANCE)
                return 0;
            return m_ip_versions[hw_ip][inst] & ~0xFFu;
        }

        uint32_t (*get_ip_versions())[HWIP_MAX_INSTANCE] { return m_ip_versions; }
        uint32_t* (*get_reg_offset())[HWIP_MAX_INSTANCE] { return m_reg_offset; }
        unsigned long get_apu_flags() const { return m_apu_flags; }
        amd_asic_type get_asic_type() const { return asic_type; }

        void test_ucode_decode()
        {
            char chip_name[32] = {};
            ucode_ip_version_decode(m_ip_versions, m_apu_flags, asic_type,
                                    MP0_HWIP, chip_name, sizeof(chip_name));
            String::Printf("PSP chip name: %s (MP0=0x%x)\n",
                            chip_name, m_ip_versions[MP0_HWIP][0]);
        }

    private:
        static constexpr uint32_t MP0_C2PMSG_33 = 0x16061;
        static constexpr uint32_t RCC_CONFIG_MEMSIZE = 0xde3;
        static constexpr uint32_t DISCOVERY_TMR_SIZE = 10 * 1024;
        static constexpr uint32_t DISCOVERY_TMR_OFFSET = 64 * 1024;
        static constexpr uint8_t IP_FIXED_SIZE = 8; 

        HarvestInfo m_harvest;
        Syscall::Pci::KernelDevice m_gpu;

        uint8_t m_gc_ip_major = 0;
        uint8_t m_gc_ip_minor = 0;
        uint8_t m_gc_ip_revision = 0;

        uint32_t m_gc_xcc_mask = 0;
        uint32_t m_sdma_mask = 0;
        uint32_t m_sdma_num = 0;
        uint32_t m_vcn_inst_mask = 0;
        uint32_t m_jpeg_inst_mask = 0;
        uint32_t m_vcn_num = 0;
        uint32_t m_num_umc = 0;
        uint32_t m_umc_node_inst_num = 0; 
        uint32_t m_vpe_num = 0;           
        uint32_t m_wafl_ver = 0;       

        
        uint8_t m_vcn_config[4] = {}; 

        uint64_t m_mall_size = 0;
        uint32_t m_mall_half_use = 0;

        
        uint32_t m_vcn_codec_disable_mask[4] = {};

        uint32_t m_nps_type = 0;
        MemRange m_mem_ranges[16] = {};
        int m_range_cnt = 0;

        IpBlock m_ip_blocks[AMD_IP_BLOCK_TYPE_COUNT] = {};
        uint32_t m_num_ip_blocks = 0;

        GpuFamily m_family = AMDGPU_FAMILY_UNKNOWN;
        bool m_is_apu = false;
        unsigned long m_apu_flags;

        amd_asic_type asic_type;

        
        bool validate_ip(uint8_t inst, uint16_t hw_id)
        {
            bool bad = false;
            if (inst >= HWIP_MAX_INSTANCE)
            {
                String::Printf("GPU: unexpected instance_number (%d) from ip discovery blob\n", inst);
                bad = true;
            }
            if (hw_id >= HW_ID_MAX)
            {
                String::Printf("GPU: unexpected hw_id (%d) from ip discovery blob\n", hw_id);
                bad = true;
            }
            return bad; 
        }

        bool read_binary()
        {
            bool ready = false;
            for (int i = 0; i < 2000; i++)
            {
                uint32_t msg = m_mem->reg_read(MP0_C2PMSG_33);
                if (msg & 0x80000000)
                {
                    ready = true;
                    String::Printf("GPU: firmware ready after %dms\n", i);
                    break;
                }
                KernelReadOnlyData *shared = (KernelReadOnlyData *)(void *)KERNEL_RO_DATA_ADDRESS;
                uint64_t last_frame = (shared->ticks * 1000) / shared->ticksPerSecond;

                while (true)
                {
                    uint64_t now = (shared->ticks * 1000) / shared->ticksPerSecond;
                    if (now - last_frame > 10){
                        break;
                    }
                }
            }

            if (!ready)
                String::Printf("GPU: firmware ready timeout, continuing anyway\n");

            uint32_t vram_mb = m_mem->reg_read(RCC_CONFIG_MEMSIZE);
            if (!vram_mb || vram_mb == 0xFFFFFFFF)
            {
                String::Printf("GPU: invalid VRAM size 0x%x\n", vram_mb);
                return false;
            }

            uint64_t vram_size = (uint64_t)vram_mb << 20;
            String::Printf("GPU: VRAM %uMB\n", vram_mb);

            uint64_t pos = vram_size - DISCOVERY_TMR_OFFSET;
            m_mem->vram_read(pos, m_buffer, DISCOVERY_TMR_SIZE);

            String::Printf("GPU: discovery binary read from 0x%x%x\n",
                               (uint32_t)(pos >> 32), (uint32_t)(pos & 0xFFFFFFFF));

            return true;
        }

        bool verify()
        {
            binary_header *bhdr = (binary_header *)m_buffer;

            if (bhdr->binary_signature != BINARY_SIGNATURE)
            {
                String::Printf("GPU: bad signature 0x%x\n", bhdr->binary_signature);
                return false;
            }

            String::Printf("GPU: binary v%d.%d size=%d\n",
                               bhdr->version_major, bhdr->version_minor, bhdr->binary_size);

            uint32_t offset = offsetof(binary_header, binary_checksum) + sizeof(bhdr->binary_checksum);
            uint8_t *data = m_buffer + offset;
            uint32_t size = bhdr->binary_size - offset;
            uint16_t sum = calculate_checksum(data, size);

            if (sum != bhdr->binary_checksum)
            {
                String::Printf("GPU: checksum mismatch got 0x%x expected 0x%x\n",
                                   sum, bhdr->binary_checksum);
                return false;
            }

            String::Printf("GPU: checksum OK\n");

            
            {
                table_info &ip_tbl = bhdr->table_list[0];
                uint16_t tbl_off = ip_tbl.offset;
                if (tbl_off)
                {
                    ip_discovery_header *ihdr = (ip_discovery_header *)(m_buffer + tbl_off);
                    if (ihdr->signature != DISCOVERY_TABLE_SIGNATURE)
                    {
                        String::Printf("GPU: bad IP table signature 0x%x\n", ihdr->signature);
                        return false;
                    }
                    uint16_t tbl_sum = calculate_checksum(m_buffer + tbl_off, ihdr->size);
                    if (tbl_sum != ip_tbl.checksum)
                    {
                        String::Printf("GPU: IP table checksum mismatch\n");
                        return false;
                    }
                }
            }

            
            {
                table_info &gc_tbl = bhdr->table_list[1];
                uint16_t tbl_off = gc_tbl.offset;
                if (tbl_off)
                {
                    gpu_info_header *ghdr = (gpu_info_header *)(m_buffer + tbl_off);
                    if (ghdr->table_id != GC_TABLE_ID)
                    {
                        String::Printf("GPU: bad GC table id 0x%x\n", ghdr->table_id);
                        return false;
                    }
                    uint16_t tbl_sum = calculate_checksum(m_buffer + tbl_off, ghdr->size);
                    if (tbl_sum != gc_tbl.checksum)
                    {
                        String::Printf("GPU: GC table checksum mismatch\n");
                        return false;
                    }
                }
            }

            {
                table_info &ht_tbl = bhdr->table_list[2];
                uint16_t tbl_off = ht_tbl.offset;
                if (tbl_off)
                {
                    harvest_info_header *hhdr = (harvest_info_header *)(m_buffer + tbl_off);
                    if (hhdr->signature != HARVEST_TABLE_SIGNATURE)
                    {
                        String::Printf("GPU: bad harvest table signature 0x%x\n", hhdr->signature);
                        return false;
                    }
                    uint16_t tbl_sum = calculate_checksum(m_buffer + tbl_off, sizeof(harvest_table));
                    if (tbl_sum != ht_tbl.checksum)
                    {
                        String::Printf("GPU: harvest table checksum mismatch\n");
                        return false;
                    }
                }
            }

            
            {
                table_info &vcn_tbl = bhdr->table_list[3]; 
                uint16_t tbl_off = vcn_tbl.offset;
                if (tbl_off)
                {
                    vcn_info_header *vhdr = (vcn_info_header *)(m_buffer + tbl_off);
                    if (vhdr->table_id != VCN_INFO_TABLE_ID)
                    {
                        String::Printf("GPU: bad VCN table id 0x%x\n", vhdr->table_id);
                        return false;
                    }
                    uint16_t tbl_sum = calculate_checksum(m_buffer + tbl_off, vhdr->size_bytes);
                    if (tbl_sum != vcn_tbl.checksum)
                    {
                        String::Printf("GPU: VCN table checksum mismatch\n");
                        return false;
                    }
                }
            }

            return true;
        }

        bool parse()
        {
            binary_header *bhdr = (binary_header *)m_buffer;
            table_info &ip_tbl = bhdr->table_list[0];

            if (!ip_tbl.offset || !ip_tbl.size)
            {
                String::Printf("GPU: no IP discovery table\n");
                return false;
            }

            ip_discovery_header *ihdr = (ip_discovery_header *)(m_buffer + ip_tbl.offset);

            if (ihdr->signature != DISCOVERY_TABLE_SIGNATURE)
            {
                String::Printf("GPU: bad IP table signature 0x%x\n", ihdr->signature);
                return false;
            }

            String::Printf("GPU: IP table v%d %d dies\n", ihdr->version, ihdr->num_dies);

            for (uint16_t i = 0; i < ihdr->num_dies; i++)
            {
                if (!parse_die(ihdr, i))
                    return false;
            }

            if (m_wafl_ver && !m_ip_versions[XGMI_HWIP][0])
                m_ip_versions[XGMI_HWIP][0] = m_wafl_ver;

            return true;
        }

        bool parse_die(ip_discovery_header *ihdr, uint16_t die_idx)
        {
            uint16_t die_offset = ihdr->dies[die_idx].die_offset;
            die_header *dhdr = (die_header *)(m_buffer + die_offset);
            uint16_t num_ips = dhdr->num_ips;
            uint16_t ip_offset = die_offset + sizeof(die_header);

            if (dhdr->die_id != die_idx)
            {
                String::Printf("GPU: invalid die id %d expected %d\n", dhdr->die_id, die_idx);
                return false;
            }

            String::Printf("die %d: %d IPs\n", dhdr->die_id, num_ips);

            for (uint16_t j = 0; j < num_ips; j++)
                ip_offset += parse_ip(ihdr, ip_offset);

            return true;
        }

        uint16_t parse_ip(ip_discovery_header *ihdr, uint16_t ip_offset)
        {
            ip_v3 *block = (ip_v3 *)(m_buffer + ip_offset);
            uint16_t hw_id = block->hw_id;
            uint8_t inst = block->instance_number;

            uint16_t stride;
            if (ihdr->base_addr_64_bit)
                stride = sizeof(ip_v3) + block->num_base_address * sizeof(uint64_t);
            else
                stride = sizeof(ip_v3) + block->num_base_address * sizeof(uint32_t);

            if (validate_ip(inst, hw_id))
                return stride;
 
            for (uint8_t k = 0; k < block->num_base_address; k++)
            {
                if (ihdr->base_addr_64_bit)
                    block->base_address[k] = (uint32_t)(((uint64_t *)block->base_address)[k] & 0x3FFFFFFF);
                
            }

            if (hw_id == VCN_HWID)
            {
                if (m_vcn_num < 4)
                {
                    m_vcn_config[m_vcn_num] = block->revision & 0xc0;
                    block->revision &= ~0xc0;
                    m_vcn_inst_mask |= (1u << inst);
                    m_jpeg_inst_mask |= (1u << inst);
                    m_vcn_num++;
                }
                else
                    String::Printf("GPU: too many VCN instances\n");
            }

            if (hw_id == SDMA0_HWID || hw_id == SDMA1_HWID ||
                hw_id == SDMA2_HWID || hw_id == SDMA3_HWID)
            {
                if (m_sdma_num < AMDGPU_MAX_SDMA_INSTANCES)
                {
                    m_sdma_num++;
                    m_sdma_mask |= (1u << inst);
                }
                else
                    String::Printf("GPU: too many SDMA instances\n");
            }

            if (hw_id == VPE_HWID)
            {
                if (m_vpe_num < AMDGPU_MAX_VPE_INSTANCES)
                    m_vpe_num++;
                else
                    String::Printf("GPU: too many VPE instances\n");
            }

            if (hw_id == UMC_HWID)
            {
                m_num_umc++;
                m_umc_node_inst_num++;
            }

            if (hw_id == GC_HWID)
            {
                m_gc_xcc_mask |= (1u << inst);
                if (inst == 0)
                {
                    m_gc_ip_major = block->major;
                    m_gc_ip_minor = block->minor;
                    m_gc_ip_revision = block->revision;
                }
            }

            if (!m_wafl_ver && hw_id == WAFLC_HWID)
                m_wafl_ver = IP_VERSION_FULL(block->major, block->minor, block->revision, 0, 0);

            uint8_t subrev = (ihdr->version >= 3) ? block->sub_revision : 0;
            uint8_t variant = (ihdr->version >= 3) ? block->variant : 0;

            for (int hw_ip = 0; hw_ip < MAX_HWIP; hw_ip++)
            {
                if (!get_hw_id_map()[hw_ip] || get_hw_id_map()[hw_ip] != hw_id)
                    continue;

                m_ip_versions[hw_ip][inst] = IP_VERSION_FULL(block->major, block->minor,
                                                             block->revision, variant, subrev);
                if (block->num_base_address > 0)
                    m_reg_offset[hw_ip][inst] = block->base_address;

                String::Printf("  [hw_id=0x%x] v%d.%d.%d base=0x%08x\n",
                                   hw_id, block->major, block->minor, block->revision,
                                   block->num_base_address > 0 ? block->base_address[0] : 0);
            }
            return stride;
        }

        bool get_gfx_info()
        {
            binary_header *bhdr = (binary_header *)m_buffer;

            uint16_t offset = bhdr->table_list[1].offset; 

            if (!offset)
            {
                String::Printf("GPU: no GC table\n");
                return false;
            }

            gc_info_v1_0 *gc = (gc_info_v1_0 *)(m_buffer + offset);

            uint16_t major = gc->header.version_major;
            uint16_t minor = gc->header.version_minor;

            String::Printf("GPU: GC info v%d.%d\n", major, minor);

            if (major == 1)
            {
                
                String::Printf("  shader engines:      %u\n", gc->gc_num_se);
                String::Printf("  sa per se:           %u\n", gc->gc_num_sa_per_se);
                String::Printf("  wgp0 per sa:         %u\n", gc->gc_num_wgp0_per_sa);
                String::Printf("  wgp1 per sa:         %u\n", gc->gc_num_wgp1_per_sa);
                String::Printf("  rb per se:           %u\n", gc->gc_num_rb_per_se);
                String::Printf("  gl2c:                %u\n", gc->gc_num_gl2c);
                String::Printf("  gprs:                %u\n", gc->gc_num_gprs);
                String::Printf("  max gs threads:      %u\n", gc->gc_num_max_gs_thds);
                String::Printf("  gs table depth:      %u\n", gc->gc_gs_table_depth);
                String::Printf("  gsprim buff depth:   %u\n", gc->gc_gsprim_buff_depth);
                String::Printf("  double offchip lds:  %u\n", gc->gc_double_offchip_lds_buffer);
                String::Printf("  wave size:           %u\n", gc->gc_wave_size);
                String::Printf("  waves per simd:      %u\n", gc->gc_max_waves_per_simd);
                String::Printf("  scratch slots per cu:%u\n", gc->gc_max_scratch_slots_per_cu);
                String::Printf("  lds size:            %u\n", gc->gc_lds_size);
                String::Printf("  sc per se:           %u\n", gc->gc_num_sc_per_se);
                String::Printf("  packer per sc:       %u\n", gc->gc_num_packer_per_sc);

                
                if (minor >= 1)
                {
                    gc_info_v1_1 *gc1 = (gc_info_v1_1 *)(m_buffer + offset);
                    String::Printf("  tcp per sa:          %u\n", gc1->gc_num_tcp_per_sa);
                    String::Printf("  sdp interfaces:      %u\n", gc1->gc_num_sdp_interface);
                    String::Printf("  tcps:                %u\n", gc1->gc_num_tcps);
                }

                
                if (minor >= 2)
                {
                    gc_info_v1_2 *gc2 = (gc_info_v1_2 *)(m_buffer + offset);
                    String::Printf("  tcp per wpg:         %u\n", gc2->gc_num_tcp_per_wpg);
                    String::Printf("  tcp l1 size:         %u\n", gc2->gc_tcp_l1_size);
                    String::Printf("  sqc per wgp:         %u\n", gc2->gc_num_sqc_per_wgp);
                    String::Printf("  l1 icache per sqc:   %u\n", gc2->gc_l1_instruction_cache_size_per_sqc);
                    String::Printf("  l1 dcache per sqc:   %u\n", gc2->gc_l1_data_cache_size_per_sqc);
                    String::Printf("  gl1c per sa:         %u\n", gc2->gc_gl1c_per_sa);
                    String::Printf("  gl1c size per inst:  %u\n", gc2->gc_gl1c_size_per_instance);
                    String::Printf("  gl2c per gpu:        %u\n", gc2->gc_gl2c_per_gpu);
                }

                
                if (minor >= 3)
                {
                    gc_info_v1_3 *gc3 = (gc_info_v1_3 *)(m_buffer + offset);
                    String::Printf("  tcp size per cu:     %u\n", gc3->gc_tcp_size_per_cu);
                    String::Printf("  tcp cache line size: %u\n", gc3->gc_tcp_cache_line_size);
                    String::Printf("  icache per sqc:      %u\n", gc3->gc_instruction_cache_size_per_sqc);
                    String::Printf("  icache line size:    %u\n", gc3->gc_instruction_cache_line_size);
                    String::Printf("  scalar cache per sqc:%u\n", gc3->gc_scalar_data_cache_size_per_sqc);
                    String::Printf("  scalar cache line:   %u\n", gc3->gc_scalar_data_cache_line_size);
                    String::Printf("  tcc size:            %u\n", gc3->gc_tcc_size);
                    String::Printf("  tcc cache line size: %u\n", gc3->gc_tcc_cache_line_size);
                }
            }
            else if (major == 2)
            {
                gc_info_v2_0 *gc2 = (gc_info_v2_0 *)(m_buffer + offset);

                
                String::Printf("  shader engines:      %u\n", gc2->gc_num_se);
                String::Printf("  cu per sh:           %u\n", gc2->gc_num_cu_per_sh);
                String::Printf("  sh per se:           %u\n", gc2->gc_num_sh_per_se);
                String::Printf("  rb per se:           %u\n", gc2->gc_num_rb_per_se);
                String::Printf("  tccs:                %u\n", gc2->gc_num_tccs);
                String::Printf("  gprs:                %u\n", gc2->gc_num_gprs);
                String::Printf("  max gs threads:      %u\n", gc2->gc_num_max_gs_thds);
                String::Printf("  gs table depth:      %u\n", gc2->gc_gs_table_depth);
                String::Printf("  gsprim buff depth:   %u\n", gc2->gc_gsprim_buff_depth);
                String::Printf("  double offchip lds:  %u\n", gc2->gc_double_offchip_lds_buffer);
                String::Printf("  wave size:           %u\n", gc2->gc_wave_size);
                String::Printf("  waves per simd:      %u\n", gc2->gc_max_waves_per_simd);
                String::Printf("  scratch slots per cu:%u\n", gc2->gc_max_scratch_slots_per_cu);
                String::Printf("  lds size:            %u\n", gc2->gc_lds_size);
                String::Printf("  sc per se:           %u\n", gc2->gc_num_sc_per_se);
                String::Printf("  packer per sc:       %u\n", gc2->gc_num_packer_per_sc);

                
                if (minor == 1)
                {
                    gc_info_v2_1 *gc21 = (gc_info_v2_1 *)(m_buffer + offset);
                    String::Printf("  tcp per sh:          %u\n", gc21->gc_num_tcp_per_sh);
                    String::Printf("  tcp size per cu:     %u\n", gc21->gc_tcp_size_per_cu);
                    String::Printf("  sdp interfaces:      %u\n", gc21->gc_num_sdp_interface);
                    String::Printf("  cu per sqc:          %u\n", gc21->gc_num_cu_per_sqc);
                    String::Printf("  icache per sqc:      %u\n", gc21->gc_instruction_cache_size_per_sqc);
                    String::Printf("  scalar cache per sqc:%u\n", gc21->gc_scalar_data_cache_size_per_sqc);
                    String::Printf("  tcc size:            %u\n", gc21->gc_tcc_size);
                }
            }
            else
            {
                String::Printf("GPU: unhandled GC major version %d\n", major);
                return false;
            }

            return true;
        }

        bool get_mall_info()
        {
            binary_header *bhdr = (binary_header *)m_buffer;
            uint16_t offset = bhdr->table_list[MALL_INFO].offset;

            if (!offset)
            {
                String::Printf("GPU: no MALL table, skipping\n");
                return true; 
            }

            mall_info_header *hdr = (mall_info_header *)(m_buffer + offset);
            uint16_t major = hdr->version_major;
            uint16_t minor = hdr->version_minor;

            String::Printf("GPU: MALL info v%d.%d\n", major, minor);

            switch (major)
            {
            case 1:
            {
                mall_info_v1_0 *v1 = (mall_info_v1_0 *)(m_buffer + offset);
                uint32_t size_per_umc = v1->mall_size_per_m;
                uint32_t m_s_present = v1->m_s_present;
                uint32_t half_use = v1->m_half_use;
                uint64_t total = 0;

                for (uint32_t u = 0; u < m_num_umc; u++)
                {
                    if (m_s_present & (1u << u))
                        total += size_per_umc * 2;
                    else if (half_use & (1u << u))
                        total += size_per_umc / 2;
                    else
                        total += size_per_umc;
                }

                m_mall_size = total;
                m_mall_half_use = half_use;
                break;
            }
            case 2:
            {
                mall_info_v2_0 *v2 = (mall_info_v2_0 *)(m_buffer + offset);
                uint32_t size_per_umc = v2->mall_size_per_umc;
                m_mall_size = (uint64_t)size_per_umc * m_num_umc;
                break;
            }
            default:
                String::Printf("GPU: unhandled MALL info v%d.%d\n", major, minor);
                return false;
            }

            String::Printf("GPU: MALL size %uMB half_use=0x%x\n",
                (uint32_t)(m_mall_size >> 20), m_mall_half_use);
            return true;
        }

        bool get_vcn_info()
        {
            binary_header *bhdr = (binary_header *)m_buffer;
            uint16_t offset = bhdr->table_list[VCN_INFO].offset;

            if (!offset)
            {
                String::Printf("GPU: no VCN info table, skipping\n");
                return true;
            }

            if (m_vcn_num > VCN_INFO_TABLE_MAX_NUM_INSTANCES)
            {
                String::Printf("GPU: too many VCN instances (%d)\n", m_vcn_num);
                return false;
            }

            vcn_info_v1_0 *v1 = (vcn_info_v1_0 *)(m_buffer + offset);
            uint16_t major = v1->header.version_major;
            uint16_t minor = v1->header.version_minor;

            String::Printf("GPU: VCN info v%d.%d\n", major, minor);

            switch (major)
            {
            case 1:
                for (uint32_t v = 0; v < m_vcn_num; v++)
                {
                    m_vcn_codec_disable_mask[v] = v1->instance_info[v].fuse_data.all_bits;
                    String::Printf("  VCN[%d] codec_disable_mask=0x%x\n",
                                       v, m_vcn_codec_disable_mask[v]);
                }
                break;
            default:
                String::Printf("GPU: unhandled VCN info v%d.%d\n", major, minor);
                return false;
            }

            return true;
        }

        
        
        bool get_nps_info(bool refresh = false)
        {
            uint16_t offset;
            nps_info_v1_0 *nps;

            
            static uint8_t nps_tmp[sizeof(nps_info_v1_0)];

            if (refresh)
            {
                
                binary_header bhdr_tmp;
                uint32_t vram_mb = m_mem->reg_read(RCC_CONFIG_MEMSIZE);
                uint64_t vram_size = (uint64_t)vram_mb << 20;
                uint64_t pos = vram_size - DISCOVERY_TMR_OFFSET;

                m_mem->vram_read(pos, (uint8_t *)&bhdr_tmp, sizeof(bhdr_tmp));

                offset = bhdr_tmp.table_list[NPS_INFO].offset;
                uint16_t checksum = bhdr_tmp.table_list[NPS_INFO].checksum;

                m_mem->vram_read(pos + offset, nps_tmp, sizeof(nps_tmp));

                nps_info_header *nhdr = (nps_info_header *)nps_tmp;
                uint16_t sum = calculate_checksum(nps_tmp, nhdr->size_bytes);
                if (sum != checksum)
                {
                    String::Printf("GPU: NPS refresh checksum mismatch\n");
                    return false;
                }

                nps = (nps_info_v1_0 *)nps_tmp;
            }
            else
            {
                binary_header *bhdr = (binary_header *)m_buffer;
                offset = bhdr->table_list[NPS_INFO].offset;

                if (!offset)
                {
                    String::Printf("GPU: no NPS info table, skipping\n");
                    return true;
                }

                nps = (nps_info_v1_0 *)(m_buffer + offset);
            }

            uint16_t major = nps->header.version_major;
            uint16_t minor = nps->header.version_minor;

            String::Printf("GPU: NPS info v%d.%d\n", major, minor);

            switch (major)
            {
            case 1:
            {
                m_nps_type = nps->nps_type;
                m_range_cnt = nps->count;

                if (m_range_cnt > 16)
                {
                    String::Printf("GPU: NPS range count %d exceeds max\n", m_range_cnt);
                    m_range_cnt = 16;
                }

                for (int i = 0; i < m_range_cnt; i++)
                {
                    m_mem_ranges[i].base_address = nps->instance_info[i].base_address;
                    m_mem_ranges[i].limit_address = nps->instance_info[i].limit_address;
                    String::Printf("  range[%d] base=0x%x%x limit=0x%x%x\n",
                        i,
                        (uint32_t)(m_mem_ranges[i].base_address >> 32),
                        (uint32_t)(m_mem_ranges[i].base_address & 0xFFFFFFFF),
                        (uint32_t)(m_mem_ranges[i].limit_address >> 32),
                        (uint32_t)(m_mem_ranges[i].limit_address & 0xFFFFFFFF));
                }

                String::Printf("GPU: NPS type=%d ranges=%d\n", m_nps_type, m_range_cnt);
                break;
            }
            default:
                String::Printf("GPU: unhandled NPS info v%d.%d\n", major, minor);
                return false;
            }

            return true;
        }

        
        void read_harvest_bit_per_ip(HarvestInfo &result, uint32_t *vcn_harvest_count)
        {
            binary_header *bhdr = (binary_header *)m_buffer;
            table_info &ip_tbl = bhdr->table_list[0];
            ip_discovery_header *ihdr = (ip_discovery_header *)(m_buffer + ip_tbl.offset);
            uint16_t num_dies = ihdr->num_dies;

            for (uint16_t i = 0; i < num_dies; i++)
            {
                uint16_t die_offset = ihdr->dies[i].die_offset;
                die_header *dhdr = (die_header *)(m_buffer + die_offset);
                uint16_t num_ips = dhdr->num_ips;
                uint16_t ip_offset = die_offset + sizeof(die_header);

                for (uint16_t j = 0; j < num_ips; j++)
                {
                    
                    
                    ip *block = (ip *)(m_buffer + ip_offset);
                    uint16_t hw_id = block->hw_id;
                    uint8_t inst = block->number_instance;

                    if (validate_ip(inst, hw_id))
                        goto next_ip;

                    if (block->harvest == 1)
                    {
                        switch (hw_id)
                        {
                        case VCN_HWID:
                            (*vcn_harvest_count)++;
                            if (inst == 0)
                            {
                                result.vcn_harvest_config |= AMDGPU_VCN_HARVEST_VCN0;
                                result.vcn_inst_mask &= ~AMDGPU_VCN_HARVEST_VCN0;
                                result.jpeg_inst_mask &= ~AMDGPU_VCN_HARVEST_VCN0;
                            }
                            else
                            {
                                result.vcn_harvest_config |= AMDGPU_VCN_HARVEST_VCN1;
                                result.vcn_inst_mask &= ~AMDGPU_VCN_HARVEST_VCN1;
                                result.jpeg_inst_mask &= ~AMDGPU_VCN_HARVEST_VCN1;
                            }
                            break;
                        case DMU_HWID:
                            result.harvest_ip_mask |= AMD_HARVEST_IP_DMU_MASK;
                            break;
                        default:
                            break;
                        }
                    }

                next_ip:
                    
                    ip_offset += sizeof(ip) - sizeof(uint32_t) + block->num_base_address * sizeof(uint32_t);
                }
            }
        }

        void read_harvest_table(HarvestInfo &result, uint32_t *vcn_harvest_count, uint32_t *umc_harvest_count)
        {
            binary_header *bhdr = (binary_header *)m_buffer;
            uint16_t offset = bhdr->table_list[2].offset; 

            if (!offset)
            {
                String::Printf("GPU: no harvest table\n");
                return;
            }

            harvest_table *ht = (harvest_table *)(m_buffer + offset);
            uint32_t umc_harvest_config = 0;

            String::Printf("GPU: harvest table sig=0x%x ver=%d\n",
                               ht->header.signature, ht->header.version);

            for (int i = 0; i < 32; i++)
            {
                
                if (ht->list[i].hw_id == 0)
                    break;

                uint16_t hw_id = ht->list[i].hw_id;
                uint8_t inst = ht->list[i].number_instance;

                String::Printf("  harvested hw_id=0x%x inst=%d\n", hw_id, inst);

                switch (hw_id)
                {
                case VCN_HWID:
                    (*vcn_harvest_count)++;
                    result.vcn_harvest_config |= (1u << inst);
                    result.jpeg_harvest_config |= (1u << inst);
                    result.vcn_inst_mask &= ~(1u << inst);
                    result.jpeg_inst_mask &= ~(1u << inst);
                    break;
                case DMU_HWID:
                    result.harvest_ip_mask |= AMD_HARVEST_IP_DMU_MASK;
                    break;
                case UMC_HWID:
                    umc_harvest_config |= (1u << inst);
                    (*umc_harvest_count)++;
                    break;
                case GC_HWID:
                    result.gc_active_mask &= ~(1u << inst);
                    break;
                case SDMA0_HWID:
                    result.sdma_active_mask &= ~(1u << inst);
                    break;
#if defined(CONFIG_ISP)
                case ISP_HWID:
                    result.isp_harvest_config |= ~(1u << inst);
                    break;
#endif
                default:
                    break;
                }
            }

            result.umc_active_mask = ((1u << m_umc_node_inst_num) - 1u) & ~umc_harvest_config;
        }

        void harvest_config_quirk(HarvestInfo &result, const Syscall::Pci::KernelDevice &gpu)
        {
            if (((m_ip_versions[UVD_HWIP][1] & ~0xFFu) == IP_VERSION(3, 0, 1)) &&
                ((m_ip_versions[GC_HWIP][0] & ~0xFFu) == IP_VERSION(10, 3, 2)))
            {
                switch (gpu.revision)
                {
                case 0xC1:
                case 0xC2:
                case 0xC3:
                case 0xC5:
                case 0xC7:
                case 0xCF:
                case 0xDF:
                    result.vcn_harvest_config |= AMDGPU_VCN_HARVEST_VCN1;
                    result.vcn_inst_mask &= ~AMDGPU_VCN_HARVEST_VCN1;
                    String::Printf("GPU: Navy Flounder VCN1 quirk applied\n");
                    break;
                default:
                    break;
                }
            }
        }

        HarvestInfo get_harvest_info(const Syscall::Pci::KernelDevice &gpu)
        {
            HarvestInfo result = {
                m_gc_xcc_mask,    
                m_sdma_mask,      
                m_vcn_inst_mask,  
                0,                
                m_jpeg_inst_mask, 
                0,                
                0,                
                0,                
            };

            uint32_t vcn_harvest_count = 0;
            uint32_t umc_harvest_count = 0;

            binary_header *bhdr = (binary_header *)m_buffer;
            ip_discovery_header *ihdr = (ip_discovery_header *)(m_buffer + bhdr->table_list[0].offset);
            uint16_t ihdr_ver = ihdr->version;

            bool old_gc = (m_gc_ip_major < 10) ||
                          (m_gc_ip_major == 10 && m_gc_ip_minor < 2);

            if (old_gc && ihdr_ver <= 2)
            {
                bool use_per_ip =
                    (gpu.deviceId == 0x731E && (gpu.revision == 0xC6 || gpu.revision == 0xC7)) ||
                    (gpu.deviceId == 0x7340 && gpu.revision == 0xC9) ||
                    (gpu.deviceId == 0x7360 && gpu.revision == 0xC7);

                if (use_per_ip)
                {
                    String::Printf("GPU: using per-ip harvest (ihdr_ver=%d)\n", ihdr_ver);
                    read_harvest_bit_per_ip(result, &vcn_harvest_count);
                }
                else
                {
                    String::Printf("GPU: old GC no harvest method applies\n");
                }
            }
            else
            {
                String::Printf("GPU: using harvest table (ihdr_ver=%d)\n", ihdr_ver);
                read_harvest_table(result, &vcn_harvest_count, &umc_harvest_count);
            }

            harvest_config_quirk(result, gpu);

            if (vcn_harvest_count == m_vcn_num)
            {
                result.harvest_ip_mask |= AMD_HARVEST_IP_VCN_MASK;
                result.harvest_ip_mask |= AMD_HARVEST_IP_JPEG_MASK;
            }

            if (umc_harvest_count < m_num_umc)
                m_umc_node_inst_num -= umc_harvest_count;

            String::Printf("GPU: gc=0x%x sdma=0x%x vcn_inst=0x%x vcn_cfg=0x%x\n",
                               result.gc_active_mask, result.sdma_active_mask,
                               result.vcn_inst_mask, result.vcn_harvest_config);
            String::Printf("GPU: jpeg_inst=0x%x jpeg_cfg=0x%x harvest_ip=0x%x\n",
                               result.jpeg_inst_mask, result.jpeg_harvest_config,
                               result.harvest_ip_mask);
            String::Printf("GPU: umc_active=0x%x%x\n",
                               (uint32_t)(result.umc_active_mask >> 32),
                               (uint32_t)(result.umc_active_mask & 0xFFFFFFFF));

            return result;
        }

        static const IpBlockFuncs *resolve_ip_funcs(AmdIpBlockType type,
                                                    uint8_t major, uint8_t minor)
        {
            (void)type;
            (void)major;
            (void)minor;
            return nullptr;
        }

        bool add_ip_block(AmdIpBlockType type, uint8_t major, uint8_t minor,
                          uint8_t rev, const char *name)
        {
            
            if (type == AMD_IP_BLOCK_TYPE_VCN &&
                (m_harvest.harvest_ip_mask & AMD_HARVEST_IP_VCN_MASK))
            {
                String::Printf("GPU: skipping VCN (harvested)\n");
                return true;
            }
            if (type == AMD_IP_BLOCK_TYPE_JPEG &&
                (m_harvest.harvest_ip_mask & AMD_HARVEST_IP_JPEG_MASK))
            {
                String::Printf("GPU: skipping JPEG (harvested)\n");
                return true;
            }

            if (m_num_ip_blocks >= AMD_IP_BLOCK_TYPE_COUNT)
            {
                String::Printf("GPU: ip block table full\n");
                return false;
            }

            IpBlock &block = m_ip_blocks[m_num_ip_blocks++];
            block.type = type;
            block.major = major;
            block.minor = minor;
            block.rev = rev;
            block.name = name;
            block.funcs = resolve_ip_funcs(type, major, minor);
            block.initialized = false;

            String::Printf("GPU: ip block[%d] <%s v%d.%d.%d> funcs=%s\n",
                               m_num_ip_blocks - 1, name, major, minor, rev,
                               block.funcs ? "ok" : "pending");
            return true;
        }

        bool init_ip_blocks()
        {
            for (uint32_t i = 0; i < m_num_ip_blocks; i++)
            {
                IpBlock &block = m_ip_blocks[i];

                if (!block.funcs)
                {
                    String::Printf("GPU: ip block[%d] <%s> skipped (not implemented)\n",
                                       i, block.name);
                    continue;
                }

                if (block.funcs->early_init && !block.funcs->early_init(this))
                {
                    String::Printf("GPU: ip block[%d] <%s> early_init failed\n",
                                       i, block.name);
                    return false;
                }

                if (block.funcs->hw_init && !block.funcs->hw_init(this))
                {
                    String::Printf("GPU: ip block[%d] <%s> hw_init failed\n",
                                       i, block.name);
                    return false;
                }

                block.initialized = true;
                String::Printf("GPU: ip block[%d] <%s> initialized\n", i, block.name);
            }
            return true;
        }

        GpuFamily detect_family()
        {
            switch (m_ip_versions[GC_HWIP][0] & ~0xFFu)
            {
            case IP_VERSION(9, 0, 0):
                return AMDGPU_FAMILY_AI;
            case IP_VERSION(9, 2, 0):
                return AMDGPU_FAMILY_AI;
            case IP_VERSION(9, 4, 0):
                return AMDGPU_FAMILY_AI;
            case IP_VERSION(9, 5, 0):
                return AMDGPU_FAMILY_AI;

            case IP_VERSION(9, 1, 0):
                return AMDGPU_FAMILY_RV; 
            case IP_VERSION(9, 3, 0):
                return AMDGPU_FAMILY_RV; 

            case IP_VERSION(10, 1, 0):
                return AMDGPU_FAMILY_NV;
            case IP_VERSION(10, 3, 0):
            {
                
                uint32_t ver = m_ip_versions[GC_HWIP][0];
                if (ver == IP_VERSION(10, 3, 1))
                    return AMDGPU_FAMILY_VGH;
                if (ver == IP_VERSION(10, 3, 3))
                    return AMDGPU_FAMILY_YC;
                if (ver == IP_VERSION(10, 3, 6))
                    return AMDGPU_FAMILY_GC_10_3_6;
                if (ver == IP_VERSION(10, 3, 7))
                    return AMDGPU_FAMILY_GC_10_3_7;
                return AMDGPU_FAMILY_NV;
            }

            case IP_VERSION(11, 0, 0):
            {
                uint32_t ver = m_ip_versions[GC_HWIP][0];
                if (ver == IP_VERSION(11, 0, 1))
                    return AMDGPU_FAMILY_GC_11_0_1;
                if (ver == IP_VERSION(11, 0, 4))
                    return AMDGPU_FAMILY_GC_11_0_1;
                return AMDGPU_FAMILY_GC_11_0_0;
            }

            case IP_VERSION(11, 5, 0):
                return AMDGPU_FAMILY_GC_11_5_0;

            case IP_VERSION(12, 0, 0):
                return AMDGPU_FAMILY_GC_12_0_0;

            default:
                return AMDGPU_FAMILY_UNKNOWN;
            }
        }

        bool detect_is_apu()
        {
            switch (m_ip_versions[GC_HWIP][0])
            {
            case IP_VERSION(9, 1, 0):  
            case IP_VERSION(9, 2, 2):  
            case IP_VERSION(9, 3, 0):  
            case IP_VERSION(10, 1, 3): 
            case IP_VERSION(10, 1, 4):
            case IP_VERSION(10, 3, 1): 
            case IP_VERSION(10, 3, 3): 
            case IP_VERSION(10, 3, 6):
            case IP_VERSION(10, 3, 7):
            case IP_VERSION(11, 0, 1): 
            case IP_VERSION(11, 0, 4):
            case IP_VERSION(11, 5, 0): 
            case IP_VERSION(11, 5, 1):
            case IP_VERSION(11, 5, 2):
            case IP_VERSION(11, 5, 3):
                return true;
            default:
                return false;
            }
        }

        int init_apu_flags(amd_asic_type asic_type, const Syscall::Pci::KernelDevice &gpu)
        {
            if (!(m_is_apu) ||
                asic_type < CHIP_RAVEN)
                return 0;

            switch (asic_type) {
            case CHIP_RAVEN:
                if (gpu.dev == 0x15dd)
                    m_apu_flags |= AMD_APU_IS_RAVEN;
                if (gpu.dev == 0x15d8)
                    m_apu_flags |= AMD_APU_IS_PICASSO;
                break;
            case CHIP_RENOIR:
                if ((gpu.dev == 0x1636) ||
                    (gpu.dev == 0x164c))
                    m_apu_flags |= AMD_APU_IS_RENOIR;
                else
                    m_apu_flags|= AMD_APU_IS_GREEN_SARDINE;
                break;
            case CHIP_VANGOGH:
                m_apu_flags |= AMD_APU_IS_VANGOGH;
                break;
            case CHIP_YELLOW_CARP:
                break;
            case CHIP_CYAN_SKILLFISH:
                if ((gpu.dev == 0x13FE) ||
                    (gpu.dev == 0x143F))
                    m_apu_flags |= AMD_APU_IS_CYAN_SKILLFISH2;
                break;
            default:
                break;
            }

            return 0;
        }

        bool set_common_ip_blocks()
        {
            switch (m_ip_versions[GC_HWIP][0] & ~0xFFu)
            {
            case IP_VERSION(9, 0, 1):
            case IP_VERSION(9, 1, 0):
            case IP_VERSION(9, 2, 1):
            case IP_VERSION(9, 2, 2):
            case IP_VERSION(9, 3, 0):
            case IP_VERSION(9, 4, 0):
            case IP_VERSION(9, 4, 1):
            case IP_VERSION(9, 4, 2):
            case IP_VERSION(9, 4, 3):
            case IP_VERSION(9, 4, 4):
            case IP_VERSION(9, 5, 0):
                return add_ip_block(AMD_IP_BLOCK_TYPE_COMMON, 2, 0, 0, "vega10_common");
            case IP_VERSION(10, 1, 10):
            case IP_VERSION(10, 1, 1):
            case IP_VERSION(10, 1, 2):
            case IP_VERSION(10, 1, 3):
            case IP_VERSION(10, 1, 4):
            case IP_VERSION(10, 3, 0):
            case IP_VERSION(10, 3, 1):
            case IP_VERSION(10, 3, 2):
            case IP_VERSION(10, 3, 3):
            case IP_VERSION(10, 3, 4):
            case IP_VERSION(10, 3, 5):
            case IP_VERSION(10, 3, 6):
            case IP_VERSION(10, 3, 7):
                return add_ip_block(AMD_IP_BLOCK_TYPE_COMMON, 2, 0, 0, "nv_common");
            case IP_VERSION(11, 0, 0):
            case IP_VERSION(11, 0, 1):
            case IP_VERSION(11, 0, 2):
            case IP_VERSION(11, 0, 3):
            case IP_VERSION(11, 0, 4):
            case IP_VERSION(11, 5, 0):
            case IP_VERSION(11, 5, 1):
            case IP_VERSION(11, 5, 2):
            case IP_VERSION(11, 5, 3):
                return add_ip_block(AMD_IP_BLOCK_TYPE_COMMON, 2, 0, 0, "soc21_common");
            case IP_VERSION(12, 0, 0):
            case IP_VERSION(12, 0, 1):
                return add_ip_block(AMD_IP_BLOCK_TYPE_COMMON, 2, 0, 0, "soc24_common");
            default:
                String::Printf("GPU: unknown common block GC=0x%x\n",
                                   m_ip_versions[GC_HWIP][0]);
                return false;
            }
        }

        bool set_gmc_ip_blocks()
        {
            switch (m_ip_versions[GC_HWIP][0] & ~0xFFu)
            {
            case IP_VERSION(9, 0, 1):
            case IP_VERSION(9, 1, 0):
            case IP_VERSION(9, 2, 1):
            case IP_VERSION(9, 2, 2):
            case IP_VERSION(9, 3, 0):
            case IP_VERSION(9, 4, 0):
            case IP_VERSION(9, 4, 1):
            case IP_VERSION(9, 4, 2):
            case IP_VERSION(9, 4, 3):
            case IP_VERSION(9, 4, 4):
            case IP_VERSION(9, 5, 0):
                return add_ip_block(AMD_IP_BLOCK_TYPE_GMC, 9, 0, 0, "gmc_v9");
            case IP_VERSION(10, 1, 10):
            case IP_VERSION(10, 1, 1):
            case IP_VERSION(10, 1, 2):
            case IP_VERSION(10, 1, 3):
            case IP_VERSION(10, 1, 4):
            case IP_VERSION(10, 3, 0):
            case IP_VERSION(10, 3, 1):
            case IP_VERSION(10, 3, 2):
            case IP_VERSION(10, 3, 3):
            case IP_VERSION(10, 3, 4):
            case IP_VERSION(10, 3, 5):
            case IP_VERSION(10, 3, 6):
            case IP_VERSION(10, 3, 7):
                return add_ip_block(AMD_IP_BLOCK_TYPE_GMC, 10, 0, 0, "gmc_v10");
            case IP_VERSION(11, 0, 0):
            case IP_VERSION(11, 0, 1):
            case IP_VERSION(11, 0, 2):
            case IP_VERSION(11, 0, 3):
            case IP_VERSION(11, 0, 4):
            case IP_VERSION(11, 5, 0):
            case IP_VERSION(11, 5, 1):
            case IP_VERSION(11, 5, 2):
            case IP_VERSION(11, 5, 3):
                return add_ip_block(AMD_IP_BLOCK_TYPE_GMC, 11, 0, 0, "gmc_v11");
            case IP_VERSION(12, 0, 0):
            case IP_VERSION(12, 0, 1):
                return add_ip_block(AMD_IP_BLOCK_TYPE_GMC, 12, 0, 0, "gmc_v12");
            default:
                String::Printf("GPU: unknown gmc block GC=0x%x\n",
                                   m_ip_versions[GC_HWIP][0]);
                return false;
            }
        }

        bool set_ih_ip_blocks()
        {
            switch (m_ip_versions[OSSSYS_HWIP][0] & ~0xFFu)
            {
            case IP_VERSION(4, 0, 0):
            case IP_VERSION(4, 0, 1):
            case IP_VERSION(4, 1, 0):
            case IP_VERSION(4, 1, 1):
            case IP_VERSION(4, 3, 0):
                return add_ip_block(AMD_IP_BLOCK_TYPE_IH, 4, 0, 0, "vega10_ih");
            case IP_VERSION(4, 2, 0):
            case IP_VERSION(4, 2, 1):
            case IP_VERSION(4, 4, 0):
            case IP_VERSION(4, 4, 2):
            case IP_VERSION(4, 4, 5):
                return add_ip_block(AMD_IP_BLOCK_TYPE_IH, 4, 2, 0, "vega20_ih");
            case IP_VERSION(5, 0, 0):
            case IP_VERSION(5, 0, 1):
            case IP_VERSION(5, 0, 2):
            case IP_VERSION(5, 0, 3):
            case IP_VERSION(5, 2, 0):
            case IP_VERSION(5, 2, 1):
                return add_ip_block(AMD_IP_BLOCK_TYPE_IH, 5, 0, 0, "navi10_ih");
            case IP_VERSION(6, 0, 0):
            case IP_VERSION(6, 0, 1):
            case IP_VERSION(6, 0, 2):
                return add_ip_block(AMD_IP_BLOCK_TYPE_IH, 6, 0, 0, "ih_v6_0");
            case IP_VERSION(6, 1, 0):
                return add_ip_block(AMD_IP_BLOCK_TYPE_IH, 6, 1, 0, "ih_v6_1");
            case IP_VERSION(7, 0, 0):
                return add_ip_block(AMD_IP_BLOCK_TYPE_IH, 7, 0, 0, "ih_v7_0");
            default:
                String::Printf("GPU: unknown ih block OSSSYS=0x%x\n",
                                   m_ip_versions[OSSSYS_HWIP][0]);
                return false;
            }
        }

        bool set_psp_ip_blocks()
        {
            switch (m_ip_versions[MP0_HWIP][0] & ~0xFFu)
            {
            case IP_VERSION(9, 0, 0):
                return add_ip_block(AMD_IP_BLOCK_TYPE_PSP, 3, 1, 0, "psp_v3_1");
            case IP_VERSION(10, 0, 0):
            case IP_VERSION(10, 0, 1):
                return add_ip_block(AMD_IP_BLOCK_TYPE_PSP, 10, 0, 0, "psp_v10_0");
            case IP_VERSION(11, 0, 0):
            case IP_VERSION(11, 0, 2):
            case IP_VERSION(11, 0, 4):
            case IP_VERSION(11, 0, 5):
            case IP_VERSION(11, 0, 9):
            case IP_VERSION(11, 0, 7):
            case IP_VERSION(11, 0, 11):
            case IP_VERSION(11, 0, 12):
            case IP_VERSION(11, 0, 13):
            case IP_VERSION(11, 5, 0):
            case IP_VERSION(11, 5, 2):
                return add_ip_block(AMD_IP_BLOCK_TYPE_PSP, 11, 0, 8, "psp_v11_0");
            case IP_VERSION(11, 0, 8):
                return add_ip_block(AMD_IP_BLOCK_TYPE_PSP, 12, 0, 0, "psp_v11_0_8");
            case IP_VERSION(11, 0, 3):
            case IP_VERSION(12, 0, 1):
                return add_ip_block(AMD_IP_BLOCK_TYPE_PSP, 12, 0, 0, "psp_v12_0");
            case IP_VERSION(13, 0, 0):
            case IP_VERSION(13, 0, 1):
            case IP_VERSION(13, 0, 2):
            case IP_VERSION(13, 0, 3):
            case IP_VERSION(13, 0, 5):
            case IP_VERSION(13, 0, 6):
            case IP_VERSION(13, 0, 7):
            case IP_VERSION(13, 0, 8):
            case IP_VERSION(13, 0, 10):
            case IP_VERSION(13, 0, 11):
            case IP_VERSION(13, 0, 12):
            case IP_VERSION(13, 0, 14):
            case IP_VERSION(14, 0, 0):
            case IP_VERSION(14, 0, 1):
            case IP_VERSION(14, 0, 4):
                return add_ip_block(AMD_IP_BLOCK_TYPE_PSP, 12, 0, 0, "psp_v13_0");
            case IP_VERSION(13, 0, 4):
                return add_ip_block(AMD_IP_BLOCK_TYPE_PSP, 12, 0, 0, "psp_v13_0_4");
            case IP_VERSION(14, 0, 2):
            case IP_VERSION(14, 0, 3):
            case IP_VERSION(14, 0, 5):
                return add_ip_block(AMD_IP_BLOCK_TYPE_PSP, 12, 0, 0, "psp_v14_0");
            default:
                String::Printf("GPU: unknown psp block MP0=0x%x\n",
                                   m_ip_versions[MP0_HWIP][0]);
                return false;
            }
        }

        bool set_smu_ip_blocks()
        {
            switch (m_ip_versions[MP1_HWIP][0] & ~0xFFu)
            {
            case IP_VERSION(9, 0, 0):
            case IP_VERSION(10, 0, 0):
            case IP_VERSION(10, 0, 1):
            case IP_VERSION(11, 0, 2):
                if (asic_type == CHIP_ARCTURUS)
                    return add_ip_block(AMD_IP_BLOCK_TYPE_SMC, 11, 0, 0, "smu_v11_0");
                else
                    return add_ip_block(AMD_IP_BLOCK_TYPE_SMC, 11, 0, 0, "pp_smu");
            case IP_VERSION(11, 0, 0):
            case IP_VERSION(11, 0, 5):
            case IP_VERSION(11, 0, 9):
            case IP_VERSION(11, 0, 7):
            case IP_VERSION(11, 0, 11):
            case IP_VERSION(11, 0, 12):
            case IP_VERSION(11, 0, 13):
            case IP_VERSION(11, 5, 0):
            case IP_VERSION(11, 5, 2):
                return add_ip_block(AMD_IP_BLOCK_TYPE_SMC, 11, 0, 0, "smu_v11_0");
            case IP_VERSION(11, 0, 8):
                if (m_apu_flags & AMD_APU_IS_CYAN_SKILLFISH2){
                    return add_ip_block(AMD_IP_BLOCK_TYPE_SMC, 11, 0, 0, "smu_v11_0");
                }
            case IP_VERSION(12, 0, 0):
            case IP_VERSION(12, 0, 1):
                return add_ip_block(AMD_IP_BLOCK_TYPE_SMC, 12, 0, 0, "smu_v12_0");
            case IP_VERSION(13, 0, 0):
            case IP_VERSION(13, 0, 1):
            case IP_VERSION(13, 0, 2):
            case IP_VERSION(13, 0, 3):
            case IP_VERSION(13, 0, 4):
            case IP_VERSION(13, 0, 5):
            case IP_VERSION(13, 0, 6):
            case IP_VERSION(13, 0, 7):
            case IP_VERSION(13, 0, 8):
            case IP_VERSION(13, 0, 10):
            case IP_VERSION(13, 0, 11):
            case IP_VERSION(13, 0, 14):
            case IP_VERSION(13, 0, 12):
                return add_ip_block(AMD_IP_BLOCK_TYPE_SMC, 13, 0, 0, "smu_v13_0");
            case IP_VERSION(14, 0, 0):
            case IP_VERSION(14, 0, 1):
            case IP_VERSION(14, 0, 2):
            case IP_VERSION(14, 0, 3):
            case IP_VERSION(14, 0, 4):
            case IP_VERSION(14, 0, 5):
                return add_ip_block(AMD_IP_BLOCK_TYPE_SMC, 14, 0, 0, "smu_v14_0");
            default:
                String::Printf("GPU: unknown smu block MP1=0x%x\n",
                                   m_ip_versions[MP1_HWIP][0]);
                return false;
            }
        }

        bool set_display_ip_blocks()
        {
            uint32_t dce = m_ip_versions[DCE_HWIP][0];
            uint32_t dci = m_ip_versions[DCI_HWIP][0];

            if (dce)
                return add_ip_block(AMD_IP_BLOCK_TYPE_DCE,
                                    (dce >> 24) & 0xFF,
                                    (dce >> 16) & 0xFF,
                                    (dce >> 8) & 0xFF, "dm");
            if (dci)
                return add_ip_block(AMD_IP_BLOCK_TYPE_DCE,
                                    (dci >> 24) & 0xFF,
                                    (dci >> 16) & 0xFF,
                                    (dci >> 8) & 0xFF, "dm_dci");
            return true; 
        }

        bool set_gc_ip_blocks()
        {
            switch (m_ip_versions[GC_HWIP][0] & ~0xFFu)
            {
            case IP_VERSION(9, 0, 1):
            case IP_VERSION(9, 1, 0):
            case IP_VERSION(9, 2, 1):
            case IP_VERSION(9, 2, 2):
            case IP_VERSION(9, 3, 0):
            case IP_VERSION(9, 4, 0):
            case IP_VERSION(9, 4, 1):
            case IP_VERSION(9, 4, 2):
                return add_ip_block(AMD_IP_BLOCK_TYPE_GFX, 9, 0, 0, "gfx_v9_0");
            case IP_VERSION(9, 4, 3):
            case IP_VERSION(9, 4, 4):
            case IP_VERSION(9, 5, 0):
                return add_ip_block(AMD_IP_BLOCK_TYPE_GFX, 9, 4, 3, "gfx_v9_4_3");
            case IP_VERSION(10, 1, 10):
            case IP_VERSION(10, 1, 2):
            case IP_VERSION(10, 1, 1):
            case IP_VERSION(10, 1, 3):
            case IP_VERSION(10, 1, 4):
            case IP_VERSION(10, 3, 0):
            case IP_VERSION(10, 3, 2):
            case IP_VERSION(10, 3, 1):
            case IP_VERSION(10, 3, 4):
            case IP_VERSION(10, 3, 5):
            case IP_VERSION(10, 3, 6):
            case IP_VERSION(10, 3, 3):
            case IP_VERSION(10, 3, 7):
                return add_ip_block(AMD_IP_BLOCK_TYPE_GFX, 10, 0, 0, "gfx_v10_0");
            case IP_VERSION(11, 0, 0):
            case IP_VERSION(11, 0, 1):
            case IP_VERSION(11, 0, 2):
            case IP_VERSION(11, 0, 3):
            case IP_VERSION(11, 0, 4):
            case IP_VERSION(11, 5, 0):
            case IP_VERSION(11, 5, 1):
            case IP_VERSION(11, 5, 2):
            case IP_VERSION(11, 5, 3):
                return add_ip_block(AMD_IP_BLOCK_TYPE_GFX, 11, 0, 0, "gfx_v11_0");
            case IP_VERSION(12, 0, 0):
            case IP_VERSION(12, 0, 1):
                return add_ip_block(AMD_IP_BLOCK_TYPE_GFX, 12, 0, 0, "gfx_v12_0");
            default:
                String::Printf("GPU: unknown gc block GC=0x%x\n",
                                   m_ip_versions[GC_HWIP][0]);
                return false;
            }
        }

        bool set_sdma_ip_blocks()
        {
            switch (m_ip_versions[SDMA0_HWIP][0] & ~0xFFu)
            {
            case IP_VERSION(4, 0, 0):
            case IP_VERSION(4, 0, 1):
            case IP_VERSION(4, 1, 0):
            case IP_VERSION(4, 1, 1):
            case IP_VERSION(4, 1, 2):
            case IP_VERSION(4, 2, 0):
            case IP_VERSION(4, 2, 2):
            case IP_VERSION(4, 4, 0):
                return add_ip_block(AMD_IP_BLOCK_TYPE_SDMA, 4, 0, 0, "sdma_v4_0");
            case IP_VERSION(4, 4, 2):
            case IP_VERSION(4, 4, 5):
            case IP_VERSION(4, 4, 4):
                return add_ip_block(AMD_IP_BLOCK_TYPE_SDMA, 4, 0, 0, "sdma_v4_4_2");
            case IP_VERSION(5, 0, 0):
            case IP_VERSION(5, 0, 1):
            case IP_VERSION(5, 0, 2):
            case IP_VERSION(5, 0, 5):
                return add_ip_block(AMD_IP_BLOCK_TYPE_SDMA, 5, 0, 0, "sdma_v5_0");
            case IP_VERSION(5, 2, 0):
            case IP_VERSION(5, 2, 2):
            case IP_VERSION(5, 2, 4):
            case IP_VERSION(5, 2, 5):
            case IP_VERSION(5, 2, 6):
            case IP_VERSION(5, 2, 3):
            case IP_VERSION(5, 2, 1):
            case IP_VERSION(5, 2, 7):
                return add_ip_block(AMD_IP_BLOCK_TYPE_SDMA, 5, 2, 0, "sdma_v5_2");
            case IP_VERSION(6, 0, 0):
            case IP_VERSION(6, 0, 1):
            case IP_VERSION(6, 0, 2):
            case IP_VERSION(6, 0, 3):
            case IP_VERSION(6, 1, 0):
            case IP_VERSION(6, 1, 1):
            case IP_VERSION(6, 1, 2):
            case IP_VERSION(6, 1, 3):
                return add_ip_block(AMD_IP_BLOCK_TYPE_SDMA, 6, 0, 0, "sdma_v6_0");
            case IP_VERSION(7, 0, 0):
            case IP_VERSION(7, 0, 1):
                return add_ip_block(AMD_IP_BLOCK_TYPE_SDMA, 7, 0, 0, "sdma_v7_0");
            default:
                String::Printf("GPU: unknown sdma block SDMA0=0x%x\n",
                                   m_ip_versions[SDMA0_HWIP][0]);
                return false;
            }
        }

        bool set_mm_ip_blocks()
        {
            
            if (m_ip_versions[VCE_HWIP][0])
            {
                switch (m_ip_versions[UVD_HWIP][0] & ~0xFFu){
                    case IP_VERSION(7, 0, 0):
                    case IP_VERSION(7, 2, 0):
                        if (!(asic_type == CHIP_VEGA20)){
                            add_ip_block(AMD_IP_BLOCK_TYPE_UVD, 7, 0, 0, "uvd_v7_0");
                        }
                        break;
                    default:
                        String::Printf("Failed to add uvd v7 ip block(UVD_HWIP:0x%x)",m_ip_versions[UVD_HWIP][0]);
                        return false;
                }
                switch (m_ip_versions[VCE_HWIP][0] & ~0xFFu){
                    case IP_VERSION(4, 0, 0):
                    case IP_VERSION(4, 1, 0):
                        if (!(asic_type == CHIP_VEGA20)){
                            add_ip_block(AMD_IP_BLOCK_TYPE_VCE, 4, 0, 0, "vce_v4_0");
                        }
                        break;
                    default:
                        String::Printf("Failed to add VCE v4 ip block(VCE_HWIP:0x%x)",m_ip_versions[VCE_HWIP][0]);
                        return false;
                }
            }
            else{
                
                switch (m_ip_versions[UVD_HWIP][0] & ~0xFFu)
                {
                case IP_VERSION(1, 0, 0):
                case IP_VERSION(1, 0, 1):
                    return add_ip_block(AMD_IP_BLOCK_TYPE_VCN, 1, 0, 0, "vcn_v1_0");
                case IP_VERSION(2, 0, 0):
                case IP_VERSION(2, 0, 2):
                case IP_VERSION(2, 2, 0):
                    add_ip_block(AMD_IP_BLOCK_TYPE_VCN, 2, 0, 0, "vcn_v2_0");
                    return add_ip_block(AMD_IP_BLOCK_TYPE_JPEG, 2, 0, 0, "jpeg_v2_0");
        		case IP_VERSION(2, 0, 3):
                    break;
                case IP_VERSION(2, 5, 0):
                    add_ip_block(AMD_IP_BLOCK_TYPE_VCN, 2, 5, 0, "vcn_v2_5");
                    return add_ip_block(AMD_IP_BLOCK_TYPE_JPEG, 2, 5, 0, "jpeg_v2_5");
                case IP_VERSION(2, 6, 0):
                    add_ip_block(AMD_IP_BLOCK_TYPE_VCN, 2, 6, 0, "vcn_v2_6");
                    return add_ip_block(AMD_IP_BLOCK_TYPE_JPEG, 2, 6, 0, "jpeg_v2_6");
                case IP_VERSION(3, 0, 0):
                case IP_VERSION(3, 0, 16):
                case IP_VERSION(3, 1, 1):
                case IP_VERSION(3, 1, 2):
                case IP_VERSION(3, 0, 2):
                    add_ip_block(AMD_IP_BLOCK_TYPE_VCN, 3, 0, 0, "vcn_v3_0");
                    return add_ip_block(AMD_IP_BLOCK_TYPE_JPEG, 3, 0, 0, "jpeg_v3_0");
        		case IP_VERSION(3, 0, 33):
                    return add_ip_block(AMD_IP_BLOCK_TYPE_VCN, 3, 0, 0, "vcn_v3_0");
                case IP_VERSION(4, 0, 0):
                case IP_VERSION(4, 0, 2):
                case IP_VERSION(4, 0, 4):
                    add_ip_block(AMD_IP_BLOCK_TYPE_VCN, 4, 0, 0, "vcn_v4_0");
                    return add_ip_block(AMD_IP_BLOCK_TYPE_JPEG, 4, 0, 0, "jpeg_v4_0");
        		case IP_VERSION(4, 0, 3):
                    add_ip_block(AMD_IP_BLOCK_TYPE_VCN, 4, 0, 0, "vcn_v4_0_3");
                    return add_ip_block(AMD_IP_BLOCK_TYPE_JPEG, 4, 0, 3, "jpeg_v4_0_3");        
        		case IP_VERSION(4, 0, 5):
                case IP_VERSION(4, 0, 6):
                    add_ip_block(AMD_IP_BLOCK_TYPE_VCN, 4, 0, 5, "vcn_v4_0_5");
                    return add_ip_block(AMD_IP_BLOCK_TYPE_JPEG, 4, 0, 5, "jpeg_v4_0_5");      
                case IP_VERSION(5, 0, 0):
                    add_ip_block(AMD_IP_BLOCK_TYPE_VCN, 5, 0, 0, "vcn_v5_0_0");
                    return add_ip_block(AMD_IP_BLOCK_TYPE_JPEG, 5, 0, 0, "jpeg_v5_0_0");
                case IP_VERSION(5, 0, 1):
                    add_ip_block(AMD_IP_BLOCK_TYPE_VCN, 5, 0, 1, "vcn_v5_0_1");
                    return add_ip_block(AMD_IP_BLOCK_TYPE_JPEG, 5, 0, 1, "jpeg_v5_0_1");
                default:
                    String::Printf("GPU: unknown vcn block UVD=0x%x\n",
                                    m_ip_versions[UVD_HWIP][0]);
                    return false;
                }
            }
            return true;
        }

        bool set_ip_blocks()
        {
            m_family = detect_family();
            if (m_family == AMDGPU_FAMILY_UNKNOWN)
            {
                String::Printf("GPU: unknown family GC=0x%x\n",
                                   m_ip_versions[GC_HWIP][0]);
                return false;
            }

            m_is_apu = detect_is_apu();

            String::Printf("GPU: family=%d is_apu=%d\n", (int)m_family, m_is_apu);

            if (!set_common_ip_blocks())
                return false;
            if (!set_gmc_ip_blocks())
                return false;
            if (!set_ih_ip_blocks())
                return false;
            if (!set_psp_ip_blocks())
                return false;
            if (!set_smu_ip_blocks())
                return false;
            if (!set_display_ip_blocks())
                return false;
            if (!set_gc_ip_blocks())
                return false;
            if (!set_sdma_ip_blocks())
                return false;
            if (!set_mm_ip_blocks())
                return false;

            String::Printf("GPU: %d ip blocks registered\n", m_num_ip_blocks);
            return true;
        }

        uint16_t calculate_checksum(uint8_t *data, uint32_t size)
        {
            uint16_t sum = 0;
            for (uint32_t i = 0; i < size; i++)
                sum += data[i];
            return sum;
        }

        GpuMemory *m_mem;
        uint8_t *m_buffer;

        uint32_t m_ip_versions[MAX_HWIP][HWIP_MAX_INSTANCE];
        uint32_t *m_reg_offset[MAX_HWIP][HWIP_MAX_INSTANCE];
    };
}