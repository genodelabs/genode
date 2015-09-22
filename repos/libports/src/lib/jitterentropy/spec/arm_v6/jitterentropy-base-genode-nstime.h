/*
 * \brief  Time backend for jitterentropy library
 * \author Josef Soentgen
 * \date   2014-08-18
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _JITTERENTROPY_ARM_V6_BASE_GENODE_NSTIME_H_
#define _JITTERENTROPY_ARM_V6_BASE_GENODE_NSTIME_H_

static inline void jent_get_nstime(__u64 *out)
{
	uint32_t t;
	asm volatile("mrc p15, 0, %0, c15, c12, 1" : "=r"(t));
	*out = t;
}

#endif /* _JITTERENTROPY_ARM_V6_BASE_GENODE_NSTIME_H */
