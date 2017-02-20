/*
 * \brief  Implementation of linux/gfp.h
 * \author Norman Feske
 * \date   2015-09-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Linux kit includes */
#include <lx_kit/addr_to_page_mapping.h>
#include <lx_kit/backend_alloc.h>
#include <lx_kit/malloc.h>
#include <lx_kit/env.h>


struct page *alloc_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page *page = (struct page *)kzalloc(sizeof(struct page), 0);

	size_t size = PAGE_SIZE << order;

	Genode::Ram_dataspace_capability ds_cap = Lx::backend_alloc(size, Genode::UNCACHED);
	page->addr = Lx_kit::env().rm().attach(ds_cap);
	page->paddr = Genode::Dataspace_client(ds_cap).phys_addr();

	if (!page->addr) {
		Genode::error("alloc_pages: ", size, " failed");
		kfree(page);
		return 0;
	}

	Lx::Addr_to_page_mapping::insert(page, ds_cap);

	return page;
}


void free_pages(unsigned long addr, unsigned int order)
{
	page * page = Lx::Addr_to_page_mapping::find_page(addr);
	if (!page) return;

	Genode::Ram_dataspace_capability cap =
		Lx::Addr_to_page_mapping::remove(page);
	if (cap.valid())
		Lx::backend_free(cap);
	kfree(page);
}


void get_page(struct page *page)
{
	atomic_inc(&page->_count);
}
