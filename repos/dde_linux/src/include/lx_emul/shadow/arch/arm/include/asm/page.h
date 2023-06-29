/*
 * \brief  Shadows Linux kernel arch/arm/include/asm/page.h
 * \author Stefan Kalkowski
 * \date   2022-03-10
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef __ASMARM_PAGE_H
#define __ASMARM_PAGE_H

#define PAGE_SHIFT 12
#define PAGE_SIZE  (_AC(1,UL) << PAGE_SHIFT)
#define PAGE_MASK  (~((1 << PAGE_SHIFT) - 1))

#ifndef __ASSEMBLY__

#include <lx_emul/page_virt.h>
#include <asm/page-nommu.h>
#include <asm-generic/pgtable-nopud.h>

/*
 * The 'virtual' member of 'struct page' is needed by 'lx_emul_virt_to_phys'
 * and 'page_to_virt'.
 */
#define WANT_PAGE_VIRTUAL

#ifdef CONFIG_ARM_LPAE
#define PTRS_PER_PMD 512
#else
#define PTRS_PER_PMD 1
#endif

#define PMD_SHIFT    21
#define PMD_SIZE     (1UL << PMD_SHIFT)
#define PMD_MASK     (~(PMD_SIZE-1))
#define PTRS_PER_PTE 512

int pud_none(pud_t pud);

typedef unsigned pteval_t;

typedef struct page *pgtable_t;

#define page_to_phys(p) __pa((p)->virtual)
#define page_to_virt(p)     ((p)->virtual)

static inline struct page *virt_to_page(void const *v) { return lx_emul_virt_to_pages(v, 1U); }

/* needed by mm/internal.h */
#define pfn_valid(pfn) (pfn != 0UL)

#include <asm/memory.h>

#endif /* !__ASSEMBLY__ */

#include <asm-generic/getorder.h>

#endif /* __ASMARM_PAGE_H */
