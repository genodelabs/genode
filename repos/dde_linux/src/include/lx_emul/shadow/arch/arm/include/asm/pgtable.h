/*
 * \brief  Shadows Linux kernel arch/arm/include/asm/pgtable.h
 * \author Stefan Kalkowski
 * \date   2022-03-10
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef __ASM_PGTABLE_H
#define __ASM_PGTABLE_H

#include <linux/const.h>
#include <linux/version.h>
#include <asm/proc-fns.h>
#include <asm-generic/pgtable-nopud.h>
#include <asm-generic/pgtable_uffd.h>

#define PGDIR_SHIFT 21

#ifndef __ASSEMBLY__

extern unsigned long empty_zero_page[PAGE_SIZE / sizeof(unsigned long)];
#define ZERO_PAGE(vaddr) ((void)(vaddr),virt_to_page(empty_zero_page))

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,5,0)
pte_t pte_mkwrite(pte_t pte, struct vm_area_struct *vma);
#else
pte_t pte_mkwrite(pte_t pte);
#endif

int pte_none(pte_t pte);
int pte_present(pte_t pte);
int pte_swp_soft_dirty(pte_t pte);
int pte_dirty(pte_t ptr);
int pte_write(pte_t ptr);

#define __pte_to_swp_entry(pte)	((swp_entry_t) { pte_val(pte) })

#define __swp_type(x)   ( lx_emul_trace_and_stop(__func__), 0 )
#define __swp_offset(x) ( lx_emul_trace_and_stop(__func__), 0 )
#define __swp_entry(type, offset) ( lx_emul_trace_and_stop(__func__), (swp_entry_t) { 0 } )
#define __swp_entry_to_pte(swp)	((pte_t) { (swp).val })

struct page *pmd_page(pmd_t pmd);

#define PAGE_KERNEL 0UL

#define VMALLOC_START 0UL
#define VMALLOC_END   0xffffffffUL

#endif /* !__ASSEMBLY__ */

#endif /* __ASM_PGTABLE_H */
