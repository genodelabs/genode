/*
 * \brief  Supplement for emulation of mm/vmalloc.c
 * \author Josef Soentgen
 * \date   2022-04-05
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#include <linux/slab.h>
#include <linux/vmalloc.h>

void vfree(const void * addr)
{
	kfree(addr);
}


void * vmalloc(unsigned long size)
{
	return kmalloc(size, GFP_KERNEL);
}


void * vzalloc(unsigned long size)
{
	return kzalloc(size, GFP_KERNEL);
}


bool is_vmalloc_addr(const void * x)
{
	return false;
}


void *__vmalloc_node_range(unsigned long size, unsigned long align,
			unsigned long start, unsigned long end, gfp_t gfp_mask,
			pgprot_t prot, unsigned long vm_flags, int node,
			const void *caller)
{
	lx_emul_trace_and_stop(__func__);
}
