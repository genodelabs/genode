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
#include <linux/version.h>
#include <lx_emul/page_virt.h>

unsigned long __alloc_pages_bulk(gfp_t gfp,int preferred_nid,
                                 nodemask_t * nodemask, int nr_pages,
                                 struct list_head * page_list, struct page ** page_array)
{
	if (page_list)
		lx_emul_trace_and_stop("__alloc_pages_bulk unsupported argument");

	{
		void const  *ptr  = lx_emul_mem_alloc_aligned(PAGE_SIZE*nr_pages, PAGE_SIZE);
		struct page *page = lx_emul_virt_to_pages(ptr, nr_pages);
		int i;

		for (i = 0; i < nr_pages; i++) {

			if (page_array[i])
				lx_emul_trace_and_stop("__alloc_pages_bulk: page_array entry not null");

			page_array[i] = page + i;
		}
	}

	return nr_pages;
}


void __free_pages(struct page * page, unsigned int order)
{
	unsigned i;
	unsigned const num_pages = (1 << order);
	void *   const virt_addr = page->virtual;

	if (atomic_read(&page->_refcount) && !atomic_dec_and_test(&page->_refcount))
		return;

	for (i = 0; i < num_pages; i++)
		lx_emul_disassociate_page_from_virt_addr(page[i].virtual);

	lx_emul_mem_free(virt_addr);
	lx_emul_mem_free(page);
}


/*
 * In earlier kernel versions, '__alloc_pages' was an inline function.
 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(5,12,0)
struct page * __alloc_pages(gfp_t gfp, unsigned int order, int preferred_nid,
                            nodemask_t * nodemask)
{
	unsigned const nr_pages = (1u << order);

	void const  *ptr  = lx_emul_mem_alloc_aligned(PAGE_SIZE*nr_pages, PAGE_SIZE);
	struct page *page = lx_emul_virt_to_pages(ptr, nr_pages);

	atomic_set(&page->_refcount, 1);

	return page;
}
#endif
