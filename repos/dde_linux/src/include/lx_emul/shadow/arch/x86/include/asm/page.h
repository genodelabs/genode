/*
 * \brief  Shadows Linux kernel asm-generic/page.h
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

#include <asm/page_types.h>

#ifdef CONFIG_X86_64
#include <asm/page_64.h>
#else
#include <asm/page_32.h>
#endif	/* CONFIG_X86_64 */

/*
 * The 'virtual' member of 'struct page' is needed by 'lx_emul_virt_to_phys'
 * and 'page_to_virt'.
 */
#define WANT_PAGE_VIRTUAL

#ifndef __ASSEMBLY__

#include <lx_emul/debug.h>
#include <lx_emul/page_virt.h>
#include <lx_emul/alloc.h>

struct page;

#include <linux/range.h>
extern struct range pfn_mapped[];
extern int nr_pfn_mapped;

static inline void clear_user_page(void *page, unsigned long vaddr,
				   struct page *pg)
{
	clear_page(page);
}

static inline void copy_user_page(void *to, void *from, unsigned long vaddr,
				  struct page *topage)
{
	copy_page(to, from);
}

typedef struct page *pgtable_t;

#define __va(x) ((void*)lx_emul_mem_virt_addr((void*)(x)))
#define __pa(x) lx_emul_mem_dma_addr((void *)(x))

#define virt_to_pfn(kaddr)	(__pa(kaddr) >> PAGE_SHIFT)
#define pfn_to_virt(pfn)	__va((pfn) << PAGE_SHIFT)
#define pfn_to_kaddr(pfn) __va((pfn) << PAGE_SHIFT)

static inline struct page *virt_to_page(void const *v) { return lx_emul_virt_to_pages(v, 1U); }
#define page_to_virt(p) ((p)->virtual)

#define virt_addr_valid(kaddr) ((unsigned long)kaddr != 0UL)

#endif /* __ASSEMBLY__ */

#include <asm/memory_model.h>
#include <asm-generic/getorder.h>

/* referenced in pgtable_64_types.h by VMALLOC_START */
extern unsigned long vmalloc_base;

#endif /* __ASM_GENERIC_PAGE_H */
