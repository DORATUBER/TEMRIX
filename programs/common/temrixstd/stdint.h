#pragma once

typedef unsigned char uint8_t;
typedef signed char int8_t;

typedef unsigned short uint16_t;
typedef short int16_t;

typedef unsigned int uint32_t;
typedef int int32_t;

typedef unsigned long uint64_t;
typedef long int64_t;

typedef unsigned long uintptr_t;

#define UINT8_MAX 0xFFU
#define INT8_MAX 0x7F
#define INT8_MIN (-0x7F - 1)

#define UINT16_MAX 0xFFFFU
#define INT16_MAX 0x7FFF
#define INT16_MIN (-0x7FFF - 1)

#define UINT32_MAX 0xFFFFFFFFU
#define INT32_MAX 0x7FFFFFFF
#define INT32_MIN (-0x7FFFFFFF - 1)

#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL
#define INT64_MAX 0x7FFFFFFFFFFFFFFFLL
#define INT64_MIN (-0x7FFFFFFFFFFFFFFFLL - 1)

#define SHRT_MAX 0x7FFF
#define SHRT_MIN (-SHRT_MAX - 1)

#define USHRT_MAX 0xFFFFU

#define INT_MAX 0x7FFFFFFF
#define INT_MIN (-INT_MAX - 1)

#define UINT_MAX 0xFFFFFFFFU

#define LONG_MAX 0x7FFFFFFFFFFFFFFFLL
#define LONG_MIN (-LONG_MAX - 1)

#define ULONG_MAX 0xFFFFFFFFFFFFFFFFULL