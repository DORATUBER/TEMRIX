#pragma once
#include "kernel/common.hpp"
#include "kernel/io.hpp"
#include "kernel/MemoryCommon.hpp"
#include "kernel/BuddyAllocator.hpp"
#include "kernel/PageTable.hpp"

namespace PCI
{
    constexpr uint16_t CONFIG_ADDRESS = 0xCF8;
    constexpr uint16_t CONFIG_DATA    = 0xCFC;

    struct KernelDevice {
        uint8_t  bus, dev, func;
        uint16_t vendorId;
        uint16_t deviceId;
        uint8_t  classCode;
        uint8_t  subclass;
        uint8_t  revision;
        uint64_t bars[6];
        uint64_t barSizes[6];
        bool     valid;
    };

    static constexpr uint32_t MAX_DEVICES = 64;

    struct MSIXTable
    {
        volatile uint32_t *base;  
        uint16_t           count; 
    };

    class Controller
    {
    public:
        uint64_t     getECAMBase(void *rsdp);

        bool     findMSIX(uint8_t bus, uint8_t dev, uint8_t func, uint8_t *out_cap_offset);
        MSIXTable getMSIXTable(uint8_t bus, uint8_t dev, uint8_t func, uint8_t cap_offset, uint64_t bars[6]);
        void     enableMSIX(uint8_t bus, uint8_t dev, uint8_t func, uint8_t cap_offset);
        void     writeMSIXEntry(MSIXTable table, uint16_t entry, uint8_t vector);
        uint32_t configRead(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)  { return read(bus, dev, func, offset); }
        void     configWrite(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val) { write(bus, dev, func, offset, val); }

        void             enumerateAll();
        uint32_t         getDeviceCount() const { return m_deviceCount; }
        const KernelDevice* getDevice(uint32_t index) const;
        uint64_t         getBarSize(uint8_t bus, uint8_t dev, uint8_t func, uint8_t barIndex);
        bool             readBar(uint8_t bus, uint8_t dev, uint8_t func, 
                                uint8_t barIndex, uint64_t* outBase, uint64_t* outSize);
    private:
        KernelDevice m_devices[MAX_DEVICES];
        uint32_t     m_deviceCount = 0;

        uint32_t read(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset);
        void     write(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t value);
    };
}