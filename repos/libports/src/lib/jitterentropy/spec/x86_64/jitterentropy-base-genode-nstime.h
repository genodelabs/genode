/*
 * \brief  Time backend for jitterentropy library
 * \author Josef Soentgen
 * \date   2014-08-18
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _JITTERENTROPY_X86_64_BASE_GENODE_NSTIME_H_
#define _JITTERENTROPY_X86_64_BASE_GENODE_NSTIME_H_

static inline void jent_get_nstime(__u64 *out)
{
	uint32_t lo, hi;
	__asm__ __volatile__ ( "rdtsc" : "=a" (lo), "=d" (hi));
	*out = (uint64_t)hi << 32 | lo;
}

#endif /* _JITTERENTROPY_X86_64_BASE_GENODE_NSTIME_H */
