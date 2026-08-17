#pragma once
#include "efi.hpp"

struct FileLoader
{
    EfiBootServices *bs;
    EfiFileProtocol *root;

    FileLoader() : bs(nullptr), root(nullptr) {}

    EfiStatus init(EfiBootServices *bootServices, EfiHandle imageHandle)
    {
        bs = bootServices;

        EfiLoadedImageProtocol *loadedImage = nullptr;
        EfiStatus s = bs->HandleProtocol(imageHandle, &LoadedImageGuid, (void **)&loadedImage);
        if (s != EfiSuccess) return s;

        EfiSimpleFileSystemProtocol *fs = nullptr;
        s = bs->HandleProtocol(loadedImage->DeviceHandle, &SfGuid, (void **)&fs);
        if (s != EfiSuccess) return s;

        return fs->OpenVolume(fs, &root);
    }

    EfiStatus getSize(const uint16_t *path, uint64_t &sizeOut)
    {
        EfiFileProtocol *file = nullptr;
        EfiStatus s = root->Open(root, &file, (uint16_t *)path, 1, 0);
        if (s != EfiSuccess) return s;

        EfiFileInfo info;
        uint64_t infoSize = sizeof(EfiFileInfo);
        s = file->GetInfo(file, &FILE_INFO_GUID, &infoSize, &info);
        file->Close(file);
        if (s != EfiSuccess) return s;

        sizeOut = info.FileSize;
        return EfiSuccess;
    }

    EfiStatus readBytes(const uint16_t *path, void *buffer, uint64_t bytesToRead)
    {
        EfiFileProtocol *file = nullptr;
        EfiStatus s = root->Open(root, &file, (uint16_t *)path, 1, 0);
        if (s != EfiSuccess) return s;

        uint64_t readSize = bytesToRead;
        s = file->Read(file, &readSize, buffer);
        file->Close(file);
        if (s != EfiSuccess) return s;
        if (readSize != bytesToRead) return EfiAborted;
        return EfiSuccess;
    }

    EfiStatus load(const uint16_t *path, uint64_t pages, uint64_t &addr, uint64_t &fileSizeOut)
    {
        EfiFileProtocol *file = nullptr;
        EfiStatus s = root->Open(root, &file, (uint16_t *)path, 1, 0);
        if (s != EfiSuccess) return s;

        EfiFileInfo info;
        uint64_t infoSize = sizeof(EfiFileInfo);
        s = file->GetInfo(file, &FILE_INFO_GUID, &infoSize, &info);
        if (s != EfiSuccess) { file->Close(file); return s; }

        uint64_t fileSize = info.FileSize;
        uint64_t neededPages = (fileSize + 0xFFF) / 0x1000;
        if (pages < neededPages) { file->Close(file); return EfiAborted; }

        s = bs->AllocatePages(0, 2, pages, &addr);
        if (s != EfiSuccess) { file->Close(file); return s; }

        uint64_t readSize = fileSize;
        s = file->Read(file, &readSize, (void *)addr);
        file->Close(file);
        if (s != EfiSuccess) return s;

        fileSizeOut = fileSize;
        return EfiSuccess;
    }
};