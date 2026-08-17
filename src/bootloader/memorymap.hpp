#pragma once

#include "efi.hpp"

struct MemoryMap
{
    uint8_t   *buf;
    uintptr_t  size;
    uintptr_t  capacity;   
    uintptr_t  key;
    uintptr_t  descSize;
    uint32_t   descVer;

    MemoryMap() : buf(nullptr), size(0), capacity(0), key(0), descSize(0), descVer(0) {}

    EfiStatus acquire(EfiBootServices *bs)
    {
        size = 0;
        EfiStatus s = bs->GetMemoryMap(&size, nullptr, &key, &descSize, &descVer);
        if (s != (EfiStatus)0x8000000000000005ULL) return s;

        size += 16 * descSize;
        capacity = size;     
        EfiStatus a = bs->AllocatePool(2, size, (void **)&buf);
        return a;
    }

    EfiStatus fetch(EfiBootServices *bs)
    {
        return bs->GetMemoryMap(&size, buf, &key, &descSize, &descVer);
    }

    EfiStatus exitBootServices(EfiBootServices *bs, EfiHandle imageHandle)
    {
        for (int i = 0; i < 5; i++)
        {
            size = capacity;
            EfiStatus s = bs->GetMemoryMap(&size, buf, &key, &descSize, &descVer);
            if (s == (EfiStatus)0x8000000000000005ULL)
            {
                bs->FreePool(buf);
                capacity = size + 2 * descSize;  
                s = bs->AllocatePool(2, capacity, (void **)&buf);
                if (s != EfiSuccess) return s;
                continue; 
            }
            if (s != EfiSuccess) return s;

            s = bs->ExitBootServices(imageHandle, key);
            if (s == EfiSuccess) return EfiSuccess;
        }
        return (EfiStatus)~0ULL;
    }
};