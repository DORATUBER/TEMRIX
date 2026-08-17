bool setupPrpList()
{
    Syscall::Memory::DmaAllocResult prpDma;
    if (Syscall::Memory::AllocDma(NVME_PAGE_SIZE, ::Memory::Read | ::Memory::Write | ::Memory::User | ::Memory::NoCache, &prpDma) != 0)
        return false;

    prpListVirt = prpDma.virt;
    prpListPhys = prpDma.phys;
    if (!prpListVirt)
        return false;

    for (int b = 0; b < (int)NVME_PAGE_SIZE; b++)
        ((uint8_t *)prpListVirt)[b] = 0;

    return true;
}

void ensureDma(uint32_t bytes)
{
    if (bytes > dmaBufSize)
    {
        Syscall::Memory::DmaAllocResult dma;
        if (Syscall::Memory::AllocDma(bytes, ::Memory::Read | ::Memory::Write | ::Memory::User | ::Memory::NoCache, &dma) != 0)
            return;
        dmaBufVirt = dma.virt;
        dmaBufPhys = dma.phys;
        dmaBufSize = bytes;
    }
}

void readSectors(uint64_t lba, uint32_t count, uint8_t *dst)
{
    uint32_t bytes = count * SECTOR_SIZE;
    ensureDma(bytes);

    PrpList list;
    list.virt  = (uint64_t *)prpListVirt;
    list.phys  = prpListPhys;
    list.count = 0;
    list.build(dmaBufPhys, bytes);

    nvme->readSectors(lba, count, dmaBufPhys, SECTOR_SIZE, list.phys);
    Memory::Copy(dst, (uint8_t *)dmaBufVirt, bytes);
}

void writeSectors(uint64_t lba, uint32_t count, uint8_t *src)
{
    uint32_t bytes = count * SECTOR_SIZE;
    ensureDma(bytes);

    Memory::Copy((uint8_t *)dmaBufVirt, src, bytes);

    PrpList list;
    list.virt  = (uint64_t *)prpListVirt;
    list.phys  = prpListPhys;
    list.count = 0;
    list.build(dmaBufPhys, bytes);

    nvme->writeSectors(lba, count, dmaBufPhys, SECTOR_SIZE, list.phys);
}