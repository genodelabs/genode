/*
 * \brief  L4lxapi library memory functions
 * \author Stefan Kalkowski
 * \date   2011-04-29
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4LX__L4LX_MEMORY_H_
#define _L4LX__L4LX_MEMORY_H_

#include <linux.h>

#ifdef __cplusplus
extern "C" {
#endif

FASTCALL int l4lx_memory_map_virtual_page(unsigned long address, unsigned long page,
                                          int map_rw);
FASTCALL int l4lx_memory_unmap_virtual_page(unsigned long address);
FASTCALL int l4lx_memory_page_mapped(unsigned long address);

#ifdef __cplusplus
}
#endif

#endif /* _L4LX__L4LX_MEMORY_H_ */
