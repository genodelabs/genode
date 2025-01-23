#ifndef _LINUX_TYPES_H
#define _LINUX_TYPES_H

#include <base/fixed_stdint.h>

typedef genode_int8_t   __s8;
typedef genode_uint8_t  __u8;
typedef genode_int16_t  __s16;
typedef genode_uint16_t __u16;
typedef genode_int32_t  __s32;
typedef genode_uint32_t __u32;
typedef genode_int64_t  __s64;
typedef genode_uint64_t __u64;

typedef __u16 __be16;
typedef __u32 __be32;

typedef __u16 __sum16;

#endif /* _LINUX_TYPES_H */
