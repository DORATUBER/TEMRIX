#pragma once
#include "efi.hpp"
#include "util.hpp"

void *findRsdp(EfiSystemTable *st)
{
    for (uint64_t i = 0; i < st->NumberOfTableEntries; i++)
    {
        EfiConfigurationTable &t = ((EfiConfigurationTable *)st->ConfigurationTable)[i];
        if (memoryCompare(&t.VendorGuid, &Acpi20Guid, 16) == 0 ||
            memoryCompare(&t.VendorGuid, &Acpi10Guid, 16) == 0)
            return t.VendorTable;
    }
    return nullptr;
}