/*
 * \brief  Replaces mm/page_alloc.c
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \date   2021-06-03
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/version.h>
#include <lx_emul/alloc.h>
#include <lx_emul/debug.h>
#include <lx_emul/page_virt.h>

/* The GFP flags allowed during early boot (mm/internal.h) */
#define GFP_BOOT_MASK (__GFP_BITS_MASK & ~(__GFP_RECLAIM|__GFP_IO|__GFP_FS))

gfp_t gfp_allowed_mask __read_mostly = GFP_BOOT_MASK;

DEFINE_STATIC_KEY_MAYBE(CONFIG_INIT_ON_ALLOC_DEFAULT_ON, init_on_alloc);
EXPORT_SYMBOL(init_on_alloc);

DEFINE_STATIC_KEY_MAYBE(CONFIG_INIT_ON_FREE_DEFAULT_ON, init_on_free);
EXPORT_SYMBOL(init_on_free);


static void prepare_compound_page(struct page *page, unsigned int order, gfp_t gfp)
{
	int i;

	if (!order || !(gfp & __GFP_COMP))
		return;

	__SetPageHead(page);
	set_compound_order(page, order);
	for (i = 1; i < compound_nr(page); i++)
		set_compound_head(&page[i], page);
}


static void liquidate_compound_page(struct page *page)
{
	int i;

	if (!PageHead(page))
		return;

	for (i = 1; i < compound_nr(page); i++)
		clear_compound_head(&page[i]);
	ClearPageHead(page);
}


static void lx_free_pages(struct page *page, bool force)
{
	void * const virt_addr = page_address(page);

	if (force)
		set_page_count(page, 0);
	else if (!put_page_testzero(page))
		return;

	liquidate_compound_page(page);

	lx_emul_mem_free(virt_addr);
}


void __free_pages(struct page * page, unsigned int order)
{
	lx_free_pages(page, false);
}


void free_pages(unsigned long addr,unsigned int order)
{
	if (addr != 0ul)
		__free_pages(virt_to_page((void *)addr), order);
}


static struct page * lx_alloc_pages(unsigned const nr_pages)
{
	void const  *ptr  = lx_emul_mem_alloc_aligned(PAGE_SIZE*nr_pages, nr_pages*PAGE_SIZE);
	struct page *page = lx_emul_virt_to_page(ptr);

	init_page_count(page);

	return page;
}


unsigned long __alloc_pages_bulk(gfp_t gfp,int preferred_nid,
                                 nodemask_t * nodemask, int nr_pages,
                                 struct list_head * page_list, struct page ** page_array)
{
	unsigned long allocated_pages = 0;
	int i;

	if (page_list)
		lx_emul_trace_and_stop("__alloc_pages_bulk unsupported argument");

	for (i = 0; i < nr_pages; i++) {

		if (page_array[i])
			lx_emul_trace_and_stop("__alloc_pages_bulk: page_array entry not null");

		page_array[i] = lx_alloc_pages(1);

		if (!page_array[i])
			break;

		++allocated_pages;
	}

	return allocated_pages;
}


/*
 * In earlier kernel versions, '__alloc_pages' was an inline function.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,13,0)
struct page * __alloc_pages_nodemask(gfp_t gfp, unsigned int order, int preferred_nid,
                                     nodemask_t * nodemask)
#else
struct page * __alloc_pages(gfp_t gfp, unsigned int order, int preferred_nid,
                            nodemask_t * nodemask)
#endif
{
	struct page *page = lx_alloc_pages(1u << order);

	if (!page)
		return 0;

	prepare_compound_page(page, order, gfp);

	return page;
}


unsigned long __get_free_pages(gfp_t gfp, unsigned int order)
{
	struct page *page = lx_alloc_pages(1u << order);

	if (!page)
		return 0;

	prepare_compound_page(page, order, gfp);

	return (unsigned long)page_address(page);
}


/*
 * Exact page allocation
 *
 * This implementation does only support alloc-free pairs that use the same
 * size and does not set the page_count of pages beyond the head page. It is
 * currently not possible to allocate individual but contiguous pages, which is
 * required to satisfy Linux semantics.
 */

void free_pages_exact(void *virt_addr, size_t size)
{
	struct page *page = lx_emul_virt_to_page(virt_addr);

	if (!page)
		return;

	lx_free_pages(page, false);
}


void *alloc_pages_exact(size_t size, gfp_t gfp_mask)
{
	size_t const nr_pages = DIV_ROUND_UP(size, PAGE_SIZE);
	struct page *page = lx_alloc_pages(nr_pages);

	if (!page)
		return NULL;

	return page_address(page);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,0,0)
void __folio_put(struct folio * folio)
{
	struct page *page = folio_page(folio, 0);

	/* should only be called if refcount is 0 */
	if (page_count(page) != 0)
		printk("%s: page refocunt not 0 for page=%px\n", __func__, page);

	lx_free_pages(&folio->page, true);
}
#endif
