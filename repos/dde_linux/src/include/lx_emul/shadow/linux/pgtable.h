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

#include <linux/init.h>
#include <asm/pgtable.h>

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

pte_t ptep_get(pte_t *ptep);

static inline int is_zero_pfn(unsigned long pfn) { return 0; }

#endif /* __LINUX_PGTABLE_H */
