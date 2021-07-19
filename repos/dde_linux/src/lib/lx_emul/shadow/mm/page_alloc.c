/*
 * \brief  Replaces mm/page_alloc.c
 * \author Stefan Kalkowski
 * \date   2021-06-03
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/gfp.h>
#include <lx_emul/page_virt.h>

struct page * __alloc_pages_nodemask(gfp_t gfp_mask, unsigned int order,
                                     int preferred_nid, nodemask_t * nodemask)
{
	unsigned const num_pages = (1 << order);
	void *   const ptr = lx_emul_mem_alloc_aligned(PAGE_SIZE*num_pages, PAGE_SIZE);

	return lx_emul_virt_to_pages(ptr, num_pages);
}


void __free_pages(struct page * page, unsigned int order)
{
	unsigned i;
	unsigned const num_pages = (1 << order);
	void *   const virt_addr = page->virtual;

	for (i = 0; i < num_pages; i++)
		lx_emul_disassociate_page_from_virt_addr(page[i].virtual);

	lx_emul_mem_free(virt_addr);
	lx_emul_mem_free(page);
}
