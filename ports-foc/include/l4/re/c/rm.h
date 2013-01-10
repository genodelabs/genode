/*
 * \brief  L4Re functions needed by L4Linux
 * \author Stefan Kalkowski
 * \date   2011-03-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4__RE__C__RM_H_
#define _L4__RE__C__RM_H_

#include <l4/re/c/dataspace.h>

#ifdef __cplusplus
extern "C" {
#endif

enum l4re_rm_flags_t {
	L4RE_RM_READ_ONLY    = 0x01,
	L4RE_RM_SEARCH_ADDR  = 0x20,
	L4RE_RM_IN_AREA      = 0x40,
	L4RE_RM_EAGER_MAP    = 0x80,
};

L4_CV int l4re_rm_find(l4_addr_t *addr, unsigned long *size, l4_addr_t *offset,
                       unsigned *flags, l4re_ds_t *m);

L4_CV int l4re_rm_attach(void **start, unsigned long size, unsigned long flags,
                         l4re_ds_t const mem, l4_addr_t offs,
                         unsigned char align);

L4_CV int l4re_rm_detach(void *addr);

L4_CV int l4re_rm_reserve_area(l4_addr_t *start, unsigned long size,
                               unsigned flags, unsigned char align);

L4_CV int l4re_rm_free_area(l4_addr_t addr);

L4_CV void l4re_rm_show_lists(void);

#ifdef __cplusplus
}
#endif

#endif /* _L4__RE__C__RM_H_ */
