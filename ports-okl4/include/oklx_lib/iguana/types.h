/*
 * \brief  Iguana API functions needed by OKLinux
 * \author Norman Feske
 * \date   2009-04-12
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKLINUX_SUPPORT__IGUANA__TYPES_H_
#define _INCLUDE__OKLINUX_SUPPORT__IGUANA__TYPES_H_

#include <l4/types.h>
#include <iguana/stdint.h>

typedef uintptr_t memsection_ref_t; /* Iguana memory area id */
typedef uintptr_t cap_t;            /* Iguana capability */
typedef L4_Word_t thread_ref_t;     /* Iguana thread id corresponds to OKL4 id */
typedef uintptr_t objref_t;         /* Iguana object reference */

#endif //_INCLUDE__OKLINUX_SUPPORT__IGUANA__TYPES_H_
