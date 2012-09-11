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

#ifndef _L4__RE__ENV_H_
#define _L4__RE__ENV_H_

#include <l4/sys/kip.h>

#include <l4/re/consts.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct l4re_env_cap_entry_t
{
	l4_cap_idx_t cap;
//	l4_umword_t flags;
//	char name[16];
} l4re_env_cap_entry_t;


typedef struct l4re_env_t
{
	l4_cap_idx_t factory;
	l4_cap_idx_t scheduler;
	l4_cap_idx_t mem_alloc;
	l4_cap_idx_t log;
	l4_cap_idx_t main_thread;
	l4_cap_idx_t rm;
	l4_fpage_t   utcb_area;
	l4_addr_t    first_free_utcb;
} l4re_env_t;

L4_CV l4re_env_cap_entry_t const * l4re_env_get_cap_l(char const *name,
                                                      unsigned l,
                                                      l4re_env_t const *e);

L4_CV l4_kernel_info_t *l4re_kip(void);

L4_CV l4re_env_t *l4re_env(void);

#ifdef __cplusplus
}
#endif

#endif /* _L4__RE__ENV_H_ */
