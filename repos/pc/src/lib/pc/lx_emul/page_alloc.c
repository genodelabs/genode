/*
 * \brief  Supplement for emulation of mm/page_alloc.c
 * \author Josef Soentgen
 * \date   2022-04-05
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#include <linux/gfp.h>
#include <linux/mm.h>


void free_pages(unsigned long addr,unsigned int order)
{
	if (addr != 0ul)
		__free_pages(virt_to_page((void *)addr), order);
}


unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page *page = alloc_pages(gfp_mask & ~__GFP_HIGHMEM, order);

	if (!page)
		return 0;

	return (unsigned long)page_address(page);
}
