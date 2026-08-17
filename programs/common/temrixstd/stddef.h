#pragma once

#include <temrixstd/stdint.h>

typedef uint64_t size_t;
typedef int64_t ssize_t;

#define SIZE_MAX 0xFFFFFFFFFFFFFFFFULL

#define offsetof(type, member) __builtin_offsetof(type, member)

#ifndef NULL
#define NULL nullptr
#endif
