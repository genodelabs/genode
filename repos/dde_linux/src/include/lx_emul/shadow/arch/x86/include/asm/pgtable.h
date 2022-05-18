/*
 * \brief  Shadows Linux kernel arch/x86/include/asm/pgtable.h
 * \author Stefan Kalkowski
 * \date   2022-01-25
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _ASM__X86__PGTABLE_H
#define _ASM__X86__PGTABLE_H

#include <linux/mem_encrypt.h>

#include <asm/page.h>
#include <asm/pgtable_types.h>

#include <lx_emul/debug.h>

#ifndef __ASSEMBLY__
#include <asm/fpu/api.h>
#endif

static inline pte_t pte_mkwrite(pte_t pte) { return pte; }

static inline bool __pkru_allows_pkey(u16 pkey, bool write)
{
	lx_emul_trace_and_stop(__func__);
}

extern unsigned long empty_zero_page[PAGE_SIZE / sizeof(unsigned long)];

#define ZERO_PAGE(vaddr) ((void)(vaddr),virt_to_page(empty_zero_page))

#ifdef CONFIG_X86_64
static inline int p4d_none(p4d_t p4d)
{
	lx_emul_trace_and_stop(__func__);
}

static inline int pud_none(pud_t pud)
{
	lx_emul_trace_and_stop(__func__);
}

#endif

#endif /*_ASM__X86__PGTABLE_H */

