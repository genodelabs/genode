/*
 * \brief  Genode base for jitterentropy
 * \author Josef Soentgen
 * \date   2014-08-18
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _JITTERENTROPY_BASE_GENODE_H_
#define _JITTERENTROPY_BASE_GENODE_H_

/* needed type definitions */
#include <base/fixed_stdint.h>

typedef __SIZE_TYPE__   size_t;
typedef genode_uint32_t uint32_t;
typedef genode_uint64_t uint64_t;
typedef uint32_t        __u32;
typedef uint64_t        __u64;

#ifndef __cplusplus
#define NULL (void*)0
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <jitterentropy-base-genode-nstime.h>

void *jent_zalloc(size_t len);
void  jent_zfree(void *ptr, unsigned int len);

static inline int jent_fips_enabled(void) { return 0; }

static inline __u64 rol64(__u64 word, unsigned int shift)
{
	return (word << shift) | (word >> (64 - shift));
}

void *memcpy(void *dest, const void *src, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* _JITTERENTROPY_BASE_GENODE_H_ */
