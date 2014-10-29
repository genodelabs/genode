/*
 * \brief  Basic type definitions expected by seL4's kernel-interface headers
 * \author Norman Feske
 * \date   2014-10-15
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SEL4__STDINT_H_
#define _INCLUDE__SEL4__STDINT_H_

#include <base/fixed_stdint.h>

typedef genode_uint8_t  uint8_t;
typedef genode_uint16_t uint16_t;
typedef genode_uint32_t uint32_t;
typedef genode_uint64_t uint64_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#endif /* _INCLUDE__SEL4__STDINT_H_ */
