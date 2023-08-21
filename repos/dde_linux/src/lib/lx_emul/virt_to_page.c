/*
 * \brief  Linux DDE virt-to-page implementation
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2021-07-02
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/slab.h>
#include <linux/page_ref.h>
#include <lx_emul/debug.h>
#include <lx_emul/page_virt.h>


struct page *lx_emul_virt_to_page(void const *virt)
{
	/* sanitize argument */
	void * const page_aligned_virt = (void *)((uintptr_t)virt & PAGE_MASK);

	struct page * const page = lx_emul_associated_page(page_aligned_virt);

	return page;
}


void lx_emul_remove_page_range(void const *virt_addr, unsigned long size)
{
	unsigned i;
	struct page *p;

	unsigned const nr_pages = DIV_ROUND_UP(size, PAGE_SIZE);

	/* sanitize argument */
	void * const page_aligned_virt = (void *)((uintptr_t)virt_addr & PAGE_MASK);

	struct page * const page = lx_emul_associated_page(page_aligned_virt);

	for (i = 0, p = page; i < nr_pages; i++, p++)
		lx_emul_disassociate_page_from_virt_addr(p->virtual);
	lx_emul_heap_free(page);
}


void lx_emul_add_page_range(void const *virt_addr, unsigned long size)
{
	unsigned i;
	struct page *p;

	/* range may comprise a partial page at the end that needs a page struct too */
	unsigned const nr_pages = DIV_ROUND_UP(size, PAGE_SIZE);
	unsigned const space    = sizeof(struct page)*nr_pages;

	struct page * const page = lx_emul_heap_alloc(space);

	for (i = 0, p = page; i < nr_pages; i++, p++) {
		p->virtual = (void *)((uintptr_t)virt_addr + i*PAGE_SIZE);
		set_page_count(p, 0);
		lx_emul_associate_page_with_virt_addr(p, p->virtual);
	}
}
