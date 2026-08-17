#include <temrixstd.h>
#include "GpuMmio.hpp"
#include "IpDiscovery.hpp"

int main(int argc, char **argv)
{
    String::Print("[gpu] starting\n");
    Syscall::Pci::KernelDevice devices[64];
    uint64_t count = Syscall::Pci::GetDevices(devices, 64);

    int foundIndex = -1;
    for (uint64_t i = 0; i < count; i++)
    {
        if (devices[i].vendorId == 0x1002 && devices[i].deviceId == 0x1638)
        {
            foundIndex = (int)i;
            break;
        }
    }

    if (foundIndex < 0)
    {
        String::Print("[gpu] no Amd Gpu found\n");
        return -1;
    }
    else
    {
        String::Printf("[gpu] Amd Gpu found at index: %d\n", foundIndex);
    }

    uint64_t bar0VirtualBaseAddress = Syscall::Memory::MapBar(foundIndex, 0);
    if (!bar0VirtualBaseAddress)
    {
        String::Printf("[gpu] failed to map Amd gpu BAR0 \n");
        return -1;
    }
    else
    {
        String::Printf("[gpu] Succeded to map Amd gpu BAR0. Address: %llx\n", bar0VirtualBaseAddress);
    }

    uint64_t bar2VirtualBaseAddress = Syscall::Memory::MapBar(foundIndex, 2);
    if (!bar2VirtualBaseAddress)
    {
        String::Printf("[gpu] failed to map Amd gpu BAR2 \n");
        return -1;
    }
    else
    {
        String::Printf("[gpu] Succeded to map Amd gpu BAR2. Address: %llx\n", bar2VirtualBaseAddress);
    }

    uint64_t bar5VirtualBaseAddress = Syscall::Memory::MapBar(foundIndex, 5);
    if (!bar5VirtualBaseAddress)
    {
        String::Printf("[gpu] failed to map Amd gpu BAR5 \n");
        return -1;
    }
    else
    {
        String::Printf("[gpu] Succeded to map Amd gpu BAR5. Address: %llx\n", bar5VirtualBaseAddress);
    }

    AMD::GpuMemory gpuMem;
    gpuMem.init((void *)bar0VirtualBaseAddress, (void *)bar2VirtualBaseAddress, (void *)bar5VirtualBaseAddress);

    AMD::IpDiscovery ipDiscovery;
    if (!ipDiscovery.init(gpuMem, devices[foundIndex]))
        String::Print("AMD GPU: IpDiscovery failed\n");
    else
    {
        String::Print("AMD GPU: IpDiscovery OK\n");
        ipDiscovery.test_ucode_decode();
    }
    return 0;
}