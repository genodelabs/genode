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

#define pgprot_decrypted(prot) (prot)

pte_t pte_swp_clear_uffd_wp(pte_t pte);
pte_t pte_swp_clear_soft_dirty(pte_t pte);

pte_t ptep_get(pte_t *ptep);

#endif /* __LINUX_PGTABLE_H */
