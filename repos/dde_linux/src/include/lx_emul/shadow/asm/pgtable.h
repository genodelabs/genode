/*
 * \brief  Shadows Linux kernel arch/.../asm/pgtable.h
 * \author Norman Feske
 * \date   2021-06-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef __ASM_PGTABLE_H
#define __ASM_PGTABLE_H

#ifndef __ASSEMBLY__

#include <asm/page.h>
#include <asm/pgtable-types.h>
#include <asm/pgtable-hwdef.h>
#include <asm/pgtable-prot.h>
#include <asm-generic/pgtable_uffd.h>
#include <linux/mm_types.h>

#include <lx_emul/debug.h>

pte_t pte_mkwrite(pte_t pte);

pte_t pte_get(pte_t pte);
pte_t pte_wrprotect(pte_t pte);
pte_t pte_modify(pte_t pte, pgprot_t prot);
pte_t pte_mkdirty(pte_t pte);

struct mm_struct;
bool mm_pmd_folded(struct mm_struct *mm); /* needed by linux/mm.h */

int pud_none(pud_t pud);

struct page *pmd_page(pmd_t pmd);

#define phys_to_ttbr(addr) (addr)

int pte_none(pte_t pte);
int pte_present(pte_t pte);
int pte_swp_soft_dirty(pte_t pte);
int pte_dirty(pte_t ptr);
int pte_write(pte_t ptr);

extern pgd_t reserved_pg_dir[PTRS_PER_PGD];
extern pgd_t swapper_pg_dir[PTRS_PER_PGD];

#define __pte_to_swp_entry(pte)	((swp_entry_t) { pte_val(pte) })
swp_entry_t __pmd_to_swp_entry(pmd_t pmd);

#define __swp_type(x)   ( lx_emul_trace_and_stop(__func__), 0 )
#define __swp_offset(x) ( lx_emul_trace_and_stop(__func__), 0 )

#define __swp_entry(type, offset) ( lx_emul_trace_and_stop(__func__), (swp_entry_t) { 0 } )

#define __swp_entry_to_pte(swp)	((pte_t) { (swp).val })
pmd_t __swp_entry_to_pmd(swp_entry_t swp);

int pmd_none(pmd_t pmd);
int pmd_present(pmd_t pmd);
int pmd_trans_huge(pmd_t pmd);
int pmd_devmap(pmd_t pmd);

int pud_devmap(pud_t pud);
int pud_trans_huge(pud_t pud);

pgprot_t pgprot_writecombine(pgprot_t prot);

pte_t mk_pte(struct page * page, pgprot_t prot);

#define HPAGE_SHIFT         PMD_SHIFT
#define HUGETLB_PAGE_ORDER  (HPAGE_SHIFT - PAGE_SHIFT)

#endif /* __ASSEMBLY__ */

#endif /* __ASM_PGTABLE_H */
