/*
 * \brief  Genode base for jitterentropy
 * \author Josef Soentgen
 * \author Christian Helmuth
 * \date   2014-08-18
 */

/*
 * Copyright (C) 2014-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _JITTERENTROPY_BASE_USER_H_
#define _JITTERENTROPY_BASE_USER_H_

/* needed type definitions */
#include <base/fixed_stdint.h>

typedef unsigned long   size_t;
typedef signed long     ssize_t;
typedef genode_uint8_t  uint8_t;
typedef genode_uint32_t uint32_t;
typedef genode_uint64_t uint64_t;
typedef uint32_t        __u32;
typedef uint64_t        __u64;
typedef genode_int64_t  __s64;

/* use gcc standard defines */
#define UINT32_MAX  __UINT32_MAX__
#define UINT32_C    __UINT32_C
#define UINT64_C    __UINT64_C

#ifndef __cplusplus
#define NULL (void*)0

#define EAGAIN     35 /* jitterentropy-health.c */
#define EOPNOTSUPP 45 /* jitterentropy-timer.h */
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <jitterentropy-base-genode-nstime.h>

void *jent_zalloc(size_t len);
void  jent_zfree(void *ptr, unsigned int len);

static inline int jent_fips_enabled(void) { return 0; }

static inline uint32_t jent_cache_size_roundup(void) { return 0; }

static inline __u64 rol64(__u64 word, unsigned int shift)
{
	return (word << shift) | (word >> (64 - shift));
}

void *jent_memcpy(void *dest, void const *src, size_t n);

void *jent_memset(void *dest, int c, size_t n);

static inline void jent_memset_secure(void *s, size_t n)
{
	jent_memset(s, 0, n);
	asm volatile ("" : : "r" (s) : "memory");
}

#ifdef __cplusplus
}
#endif

#endif /* _JITTERENTROPY_BASE_USER_H_ */
