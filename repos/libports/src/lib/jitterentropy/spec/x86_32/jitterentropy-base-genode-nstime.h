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

#ifndef _JITTERENTROPY_X86_32_BASE_GENODE_NSTIME_H_
#define _JITTERENTROPY_X86_32_BASE_GENODE_NSTIME_H_

static inline void jent_get_nstime(__u64 *out)
{
       uint64_t t;
       __asm__ __volatile__ ( "rdtsc" : "=A" (t));
       *out = t;
}

#endif /* _JITTERENTROPY_X86_32_BASE_GENODE_NSTIME_H */
