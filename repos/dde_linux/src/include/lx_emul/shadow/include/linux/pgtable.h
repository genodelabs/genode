/*
 * \brief  Shadows Linux kernel linux/pgtable.h
 * \author Norman Feske
 * \date   2021-06-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef __LINUX_PGTABLE_H
#define __LINUX_PGTABLE_H

#include <linux/pfn.h>
#include <asm/pgtable.h>

#ifndef __ASSEMBLY__

#ifndef FIRST_USER_ADDRESS
#define FIRST_USER_ADDRESS	0UL
#endif

#ifndef USER_PGTABLES_CEILING
#define USER_PGTABLES_CEILING	0UL
#endif

#ifndef MAX_PTRS_PER_PTE
#define MAX_PTRS_PER_PTE PTRS_PER_PTE
#endif

pmd_t *pmd_offset(pud_t *pud, unsigned long address);
pmd_t  pmd_swp_clear_soft_dirty(pmd_t pmd);
int    pmd_swp_soft_dirty(pmd_t pmd);

void __init pgtable_cache_init(void);

#ifndef pgprot_device
#define pgprot_device(prot)  (prot)
#endif

#ifndef pgprot_decrypted
#define pgprot_decrypted(prot) (prot)
#endif

#ifndef pgprot_modify
#define pgprot_modify pgprot_modify
pgprot_t pgprot_modify(pgprot_t oldprot, pgprot_t newprot);
#endif

#ifndef mm_pud_folded
#define mm_pud_folded(mm)	__is_defined(__PAGETABLE_PUD_FOLDED)
#endif

#ifndef mm_pmd_folded
#define mm_pmd_folded(mm)	__is_defined(__PAGETABLE_PMD_FOLDED)
#endif

#ifndef pud_offset
static inline pud_t *pud_offset(p4d_t *p4d, unsigned long address)
{
	return 0;
}
#define pud_offset pud_offset
#endif

pte_t pte_swp_clear_uffd_wp(pte_t pte);
pte_t pte_swp_clear_soft_dirty(pte_t pte);
pte_t pte_swp_mksoft_dirty(pte_t pte);
pte_t pte_swp_mkuffd_wp(pte_t pte);
pte_t pte_swp_mkexclusive(pte_t pte);

int   pte_swp_uffd_wp(pte_t pte);

static inline pte_t pte_clear_soft_dirty(pte_t pte) { return pte; }

unsigned long pte_pfn(pte_t pte);

int   pte_young(pte_t pte);
pte_t pte_mkclean(pte_t pte);
pte_t pte_mkold(pte_t pte);

#define __HAVE_ARCH_PTE_SAME
int pte_same(pte_t a, pte_t b);

pte_t pte_advance_pfn(pte_t pte, unsigned long nr);
#define pte_advance_pfn pte_advance_pfn

pte_t ptep_get(pte_t *ptep);

static inline int is_zero_pfn(unsigned long pfn) { return 0; }

#define pte_unmap(pte) ((void)(pte))

static inline int pte_swp_exclusive(pte_t pte) { return false; }

static inline pte_t pte_swp_clear_exclusive(pte_t pte) { return pte; }

#ifndef pte_batch_hint
static inline unsigned int pte_batch_hint(pte_t *ptep, pte_t pte)
{
	return 1;
}

static inline int pte_soft_dirty(pte_t pte)
{
	return 0;
}

static inline int pmd_soft_dirty(pmd_t pmd)
{
	return 0;
}

#endif

#endif /* __ASSEMBLY__ */

#endif /* __LINUX_PGTABLE_H */
