/*
 * \brief  Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2014-08-21
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/*******************
 ** linux/types.h **
 *******************/

typedef genode_int8_t   int8_t;
typedef genode_uint8_t  uint8_t;
typedef genode_int16_t  int16_t;
typedef genode_uint16_t uint16_t;
typedef genode_int32_t  int32_t;
typedef genode_uint32_t uint32_t;
typedef __SIZE_TYPE__   size_t;
typedef genode_int64_t  int64_t;
typedef genode_uint64_t uint64_t;

typedef uint32_t      uint;
typedef unsigned long ulong;

typedef int8_t   s8;
typedef uint8_t  u8;
typedef int16_t  s16;
typedef uint16_t u16;
typedef int32_t  s32;
typedef uint32_t u32;
typedef int64_t  s64;
typedef uint64_t u64;

typedef int8_t   __s8;
typedef uint8_t  __u8;
typedef int16_t  __s16;
typedef uint16_t __u16;
typedef int32_t  __s32;
typedef uint32_t __u32;
typedef int64_t  __s64;
typedef uint64_t __u64;

struct list_head {
	struct list_head *next, *prev;
};

struct hlist_head {
	struct hlist_node *first;
};

struct hlist_node {
	struct hlist_node *next, **pprev;
};

#ifndef __cplusplus
typedef _Bool bool;
enum { true = 1, false = 0 };
#endif /* __cplusplus */

#ifndef NULL
#ifdef __cplusplus
#define NULL nullptr
#else
#define NULL ((void *)0)
#endif /* __cplusplus */
#endif /* NULL */

typedef unsigned       gfp_t;
typedef unsigned long  dma_addr_t;
typedef unsigned long  pgoff_t;
typedef long long      loff_t;
typedef long           ssize_t;
typedef unsigned long  uintptr_t;
typedef int            dev_t;
typedef size_t         resource_size_t;
typedef long           off_t;
typedef int            pid_t;
typedef unsigned       fmode_t;
typedef u32            uid_t;
typedef u32            gid_t;
typedef unsigned       kuid_t;
typedef unsigned       kgid_t;
typedef size_t         __kernel_size_t;
typedef long           __kernel_time_t;
typedef long           __kernel_suseconds_t;
typedef unsigned short umode_t;
typedef __u16          __be16;
typedef __u32          __be32;
typedef long           clock_t;

#ifndef __cplusplus
typedef u16            wchar_t;
#endif

/*
 * XXX 'mode_t' is 'unsigned int' on x86_64
 */
typedef unsigned short mode_t;

typedef unsigned slab_flags_t;
