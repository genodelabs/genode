#ifndef _LINUX_TYPES_H
#define _LINUX_TYPES_H

#include <platform/types.h>

typedef signed char    __s8;
typedef unsigned char  __u8;
typedef signed short   __s16;
typedef unsigned short __u16;
typedef signed int     __s32;
typedef unsigned int   __u32;
typedef int64_t        __s64;
typedef uint64_t       __u64;

typedef __u16 __be16;
typedef __u32 __be32;

typedef __u16 __sum16;

#endif /* _LINUX_TYPES_H */
