/*
 * \brief  L4Re functions needed by L4Linux
 * \author Stefan Kalkowski
 * \date   2011-03-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4__UTIL__ATOMIC_H_
#define _L4__UTIL__ATOMIC_H_

#include <l4/sys/compiler.h>
#include <l4/sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

L4_CV int l4util_cmpxchg(volatile l4_umword_t * dest,
                         l4_umword_t cmp_val, l4_umword_t new_val);

#ifdef __cplusplus
}
#endif

#endif /* _L4__UTIL__ATOMIC_H_ */
