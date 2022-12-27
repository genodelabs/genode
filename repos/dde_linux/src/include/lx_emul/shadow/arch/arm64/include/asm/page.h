/*
 * \brief  Shadows Linux kernel asm/page.h
 * \author Norman Feske
 * \date   2021-06-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef __ASM_GENERIC_PAGE_H
#define __ASM_GENERIC_PAGE_H

#ifndef __ASSEMBLY__

#include <asm/page-def.h> /* for PAGE_SHIFT */
#include <asm/pgtable-types.h>

/*
 * The 'virtual' member of 'struct page' is needed by 'lx_emul_virt_to_phys'
 * and 'page_to_virt'.
 */
#define WANT_PAGE_VIRTUAL

#define clear_page(page)	memset((page), 0, PAGE_SIZE)
#define copy_page(to,from)	memcpy((to), (from), PAGE_SIZE)

#define clear_user_page(page, vaddr, pg)	clear_page(page)
#define copy_user_page(to, from, vaddr, pg)	copy_page(to, from)

struct page;

typedef struct page *pgtable_t;

/* needed by mm/internal.h */
#define pfn_valid(pfn) (pfn != 0UL)

#define virt_addr_valid(kaddr) (kaddr != 0UL)

#endif /* __ASSEMBLY__ */

#include <asm-generic/getorder.h>

#endif /* __ASM_GENERIC_PAGE_H */
