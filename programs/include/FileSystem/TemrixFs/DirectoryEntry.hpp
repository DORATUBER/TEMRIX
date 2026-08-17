#pragma once

#include <temrixstd.h>

#define TEMRIXFS_DIRENT_NAME_LEN  100

#define TEMRIXFS_DIRENT_TYPE_FREE 0 
#define TEMRIXFS_DIRENT_TYPE_FILE 1
#define TEMRIXFS_DIRENT_TYPE_DIR  2

struct DirEntry
{
    uint8_t  type;
    uint8_t  nameLen;
    uint16_t reserved;
    uint32_t reserved1;
    uint64_t headerLba; 
    char     name[TEMRIXFS_DIRENT_NAME_LEN];
};