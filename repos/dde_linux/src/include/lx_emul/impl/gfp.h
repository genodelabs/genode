/*
 * \brief  Implementation of linux/gfp.h
 * \author Norman Feske
 * \date   2015-09-09
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <lx_emul/impl/internal/addr_to_page_mapping.h>
#include <lx_emul/impl/internal/malloc.h>


struct page *alloc_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page *page = (struct page *)kzalloc(sizeof(struct page), 0);

	size_t size = PAGE_SIZE << order;

	page->addr = Lx::Malloc::dma().alloc(size, 12);

	if (!page->addr) {
		PERR("alloc_pages: %zu failed", size);
		kfree(page);
		return 0;
	}

	Lx::Addr_to_page_mapping::insert(page);

	atomic_set(&page->_count, 1);

	return page;
}


void get_page(struct page *page)
{
	atomic_inc(&page->_count);
}
