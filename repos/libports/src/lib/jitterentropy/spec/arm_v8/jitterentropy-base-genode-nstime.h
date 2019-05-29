/*
 * \brief  Time backend for jitterentropy library
 * \author Sebastian Sumpf
 * \date   2014-05-29
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _JITTERENTROPY_ARM_V8__BASE_GENODE_NSTIME_H_
#define _JITTERENTROPY_ARM_V8__BASE_GENODE_NSTIME_H_

static inline void jent_get_nstime(__u64 *out)
{
	uint64_t t;
	/* cycle counter */
	asm volatile("mrs %0, pmccntr_el0" : "=r" (t));
	*out = t;
}

#endif /* _JITTERENTROPY_ARM_V8_BASE_GENODE_NSTIME_H */
