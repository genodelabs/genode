/*
 * \brief  Linux DDE virt-to-page implementation
 * \author Norman Feske
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
#include <lx_emul/page_virt.h>


struct page *lx_emul_virt_to_pages(void const *virt, unsigned count)
{
	/* sanitize argument */
	void * const page_aligned_virt = (void *)((uintptr_t)virt & PAGE_MASK);

	struct page *page = lx_emul_associated_page(page_aligned_virt, 1);

	if (!page) {
		unsigned i;
		struct page * p = kzalloc(sizeof(struct page)*count, 0);
		page = p;
		for (i = 0; i < count; i++, p++) {
			p->virtual = (void*)((uintptr_t)page_aligned_virt + i*PAGE_SIZE);
			init_page_count(p);
			lx_emul_associate_page_with_virt_addr(p, p->virtual);
		}

	}

	/* consistency check */
	if (page_aligned_virt != page->virtual)
		BUG();

	return page;
}


void lx_emul_forget_pages(void const *virt, unsigned long size)
{
	for (;;) {
		struct page *page = lx_emul_associated_page(virt, size);
		if (!page)
			return;

		lx_emul_disassociate_page_from_virt_addr(page->virtual);
		kfree(page);
	}
}


#define LX_EMUL_ASSERT(cond) { if (!(cond)) {\
	printk("assertion failed at line %d: %s\n", __LINE__, #cond); \
	lx_emul_trace_and_stop("abort"); } }


void lx_emul_associate_page_selftest()
{
	struct page *p1 = (struct page *)1;
	struct page *p2 = (struct page *)2;
	struct page *p3 = (struct page *)3;

	void *v1 = (void *)0x11000;
	void *v2 = (void *)0x12000;
	void *v3 = (void *)0x13000;

	lx_emul_associate_page_with_virt_addr(p1, v1);
	lx_emul_associate_page_with_virt_addr(p2, v2);
	lx_emul_associate_page_with_virt_addr(p3, v3);

	LX_EMUL_ASSERT(lx_emul_associated_page(v1, 1) == p1);
	LX_EMUL_ASSERT(lx_emul_associated_page(v2, 1) == p2);
	LX_EMUL_ASSERT(lx_emul_associated_page(v3, 1) == p3);

	LX_EMUL_ASSERT(lx_emul_associated_page((void *)((uintptr_t)v1 + 4095), 1) == p1);
	LX_EMUL_ASSERT(lx_emul_associated_page((void *)((uintptr_t)v1 - 1), 1) == NULL);
	LX_EMUL_ASSERT(lx_emul_associated_page((void *)((uintptr_t)v2 & PAGE_MASK), 1) == p2);

	LX_EMUL_ASSERT(lx_emul_associated_page((void *)0x10000, 0x10000) == p2);
	lx_emul_disassociate_page_from_virt_addr(v2);
	LX_EMUL_ASSERT(lx_emul_associated_page((void *)0x10000, 0x10000) == p3);
	lx_emul_disassociate_page_from_virt_addr(v3);
	LX_EMUL_ASSERT(lx_emul_associated_page((void *)0x10000, 0x10000) == p1);
	lx_emul_disassociate_page_from_virt_addr(v1);
	LX_EMUL_ASSERT(lx_emul_associated_page((void *)0x10000, 0x10000) == NULL);
}
