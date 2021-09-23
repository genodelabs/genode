/*
 * \brief  Replaces mm/memblock.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/slab.h>
#include <linux/memblock.h>
#include <lx_emul/alloc.h>

void * __init memblock_alloc_try_nid(phys_addr_t size,
                                     phys_addr_t align,
                                     phys_addr_t min_addr,
                                     phys_addr_t max_addr,
                                     int nid)
{
	align = max(align, (phys_addr_t)KMALLOC_MIN_SIZE);
	return lx_emul_mem_alloc_aligned(size, align);
}
