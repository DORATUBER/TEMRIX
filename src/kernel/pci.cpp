#include "pci.hpp"
#include "lapic.hpp"
#include "Serial.hpp"

namespace PCI
{
    uint32_t Controller::read(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset)
    {
        uint32_t address = (1 << 31) | (bus << 16) | (device << 11) | (func << 8) | (offset & 0xFC);
        outl(CONFIG_ADDRESS, address);
        return inl(CONFIG_DATA);
    }

    void Controller::write(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t value)
    {
        uint32_t address = (1 << 31) | (bus << 16) | (device << 11) | (func << 8) | (offset & 0xFC);
        outl(CONFIG_ADDRESS, address);
        outl(CONFIG_DATA, value);
    }

    uint64_t Controller::getECAMBase(void *rsdp)
    {
        uint8_t *r = (uint8_t *)rsdp;

        if (r[0] != 'R' || r[1] != 'S' || r[2] != 'D' || r[3] != ' ' ||
            r[4] != 'P' || r[5] != 'T' || r[6] != 'R' || r[7] != ' ')
            return 0;

        uint8_t  revision = r[15];
        uint8_t *sdt;
        uint32_t length, num_entries;
        int      ptr_size;

        if (revision >= 2)
        {
            uint64_t xsdt_addr = *(uint64_t *)(r + 0x18);
            sdt         = (uint8_t *)xsdt_addr;
            length      = *(uint32_t *)(sdt + 4);
            num_entries = (length - 36) / 8;
            ptr_size    = 8;
        }
        else
        {
            uint32_t rsdt_addr = *(uint32_t *)(r + 0x10);
            sdt         = (uint8_t *)(uint64_t)rsdt_addr;
            length      = *(uint32_t *)(sdt + 4);
            num_entries = (length - 36) / 4;
            ptr_size    = 4;
        }

        if (sdt[0] == 0)
            return 0;

        for (uint32_t i = 0; i < num_entries; i++)
        {
            uint64_t table_addr = (ptr_size == 8)
                                      ? *(uint64_t *)(sdt + 36 + i * 8)
                                      : (uint64_t)(*(uint32_t *)(sdt + 36 + i * 4));

            if (table_addr == 0)
                continue;

            uint8_t *table = (uint8_t *)table_addr;
            if (table[0] == 'M' && table[1] == 'C' && table[2] == 'F' && table[3] == 'G')
                return *(uint64_t *)(table + 44);
        }

        return 0;
    }

    bool Controller::findMSIX(uint8_t bus, uint8_t dev, uint8_t func,
                            uint8_t *out_cap_offset)
    {
        uint32_t status = read(bus, dev, func, 0x04);
        if (!((status >> 20) & 1))
            return false;

        uint8_t cap = read(bus, dev, func, 0x34) & 0xFF;

        while (cap)
        {
            uint32_t dword = read(bus, dev, func, cap);
            uint8_t  id    = dword & 0xFF;
            uint8_t  next  = (dword >> 8) & 0xFF;

            if (id == 0x11) 
            {
                *out_cap_offset = cap;
                return true;
            }
            cap = next;
        }
        return false;
    }

    MSIXTable Controller::getMSIXTable(uint8_t bus, uint8_t dev, uint8_t func,
                                        uint8_t cap_offset, uint64_t bars[6])
    {
        MSIXTable result = {};

        uint32_t ctrl    = read(bus, dev, func, cap_offset);
        result.count     = ((ctrl >> 16) & 0x7FF) + 1;

        uint32_t table_dword = read(bus, dev, func, cap_offset + 4);
        uint8_t  bir         = table_dword & 0x7;
        uint32_t table_off   = table_dword & ~0x7u;

        Serial::printf("[msix] ctrl=0x%08x table_dword=0x%08x bir=%u table_off=0x%x bars[bir]=0x%llx\n",
                    ctrl, table_dword, bir, table_off, bars[bir]);

        result.base = (volatile uint32_t*)(Memory::phys_to_virt(bars[bir]) + table_off);
        return result;
    }

    void Controller::enableMSIX(uint8_t bus, uint8_t dev, uint8_t func,
                                uint8_t cap_offset)
    {
        uint32_t dword = read(bus, dev, func, cap_offset);
        write(bus, dev, func, cap_offset, (dword | (1u << 31)) & ~(1u << 30));
    }

    void Controller::writeMSIXEntry(MSIXTable table, uint16_t entry, uint8_t vector)
    {
        volatile uint32_t *e = table.base + (entry * 4);

        e[0] = Hardware::LAPIC::MSI_ADDRESS; 
        e[1] = 0;                       
        e[2] = vector;                     
        e[3] = 0;                       
    }

    bool Controller::readBar(uint8_t bus, uint8_t dev, uint8_t func,
                            uint8_t barIndex, uint64_t* outBase, uint64_t* outSize)
    {
        if (barIndex >= 6) return false;
        uint8_t offset = 0x10 + barIndex * 4;

        uint32_t lo = read(bus, dev, func, offset);
        bool is64 = ((lo & 0x6) == 0x4) && (barIndex < 5);

        uint64_t base;
        if (is64) {
            uint32_t hi = read(bus, dev, func, offset + 4);
            base = (lo & ~(uint64_t)0xF) | ((uint64_t)hi << 32);
        } else {
            base = lo & ~(uint64_t)0xF;
        }

        write(bus, dev, func, offset, 0xFFFFFFFF);
        if (is64) write(bus, dev, func, offset + 4, 0xFFFFFFFF);

        uint32_t size_lo = read(bus, dev, func, offset);

        uint64_t size;
        if (is64) {
            uint32_t size_hi = read(bus, dev, func, offset + 4);
            uint64_t size_mask = (size_lo & ~(uint64_t)0xF) | ((uint64_t)size_hi << 32);
            size = ~size_mask + 1;
        } else {
            uint32_t size_mask = size_lo & ~0xFu;
            size = (~size_mask + 1) & 0xFFFFFFFF;
        }

        write(bus, dev, func, offset, lo);
        if (is64) write(bus, dev, func, offset + 4, (uint32_t)(base >> 32));

        *outBase = base;
        *outSize = size;
        return base != 0;
    }

    void Controller::enumerateAll()
    {
        m_deviceCount = 0;

        for (uint16_t bus = 0; bus < 256 && m_deviceCount < MAX_DEVICES; bus++) {
            for (uint8_t dev = 0; dev < 32 && m_deviceCount < MAX_DEVICES; dev++) {
                for (uint8_t func = 0; func < 8 && m_deviceCount < MAX_DEVICES; func++) {
                    uint32_t id = read(bus, dev, func, 0);
                    if (id == 0xFFFFFFFF) continue;

                    uint32_t class_info = read(bus, dev, func, 8);

                    KernelDevice& d = m_devices[m_deviceCount];
                    d.bus       = (uint8_t)bus;
                    d.dev       = dev;
                    d.func      = func;
                    d.vendorId  = id & 0xFFFF;
                    d.deviceId  = (id >> 16) & 0xFFFF;
                    d.classCode = (class_info >> 24) & 0xFF;
                    d.subclass  = (class_info >> 16) & 0xFF;
                    d.revision  = class_info & 0xFF;
                    d.valid     = true;

                    write(bus, dev, func, 0x04, read(bus, dev, func, 0x04) | 0x6);

                    for (uint8_t i = 0; i < 6; ) {
                        uint8_t offset = 0x10 + i * 4;
                        uint32_t lo = read(bus, dev, func, offset);
                        bool is64 = ((lo & 0x6) == 0x4) && (i < 5);

                        readBar(bus, dev, func, i, &d.bars[i], &d.barSizes[i]);

                        if (is64) {
                            d.bars[i + 1]     = 0;
                            d.barSizes[i + 1] = 0;
                            i += 2;
                        } else {
                            i += 1;
                        }
                    }
                    m_deviceCount++;
                }
            }
        }
    }

    const KernelDevice* Controller::getDevice(uint32_t index) const
    {
        if (index >= m_deviceCount) return nullptr;
        return &m_devices[index];
    }
}

