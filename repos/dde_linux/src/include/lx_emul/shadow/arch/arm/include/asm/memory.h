/*
 * \brief  Shadows Linux kernel arch/arm/include/asm/memory.h
 * \author Stefan Kalkowski
 * \date   2022-03-10
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef __ASM_MEMORY_H
#define __ASM_MEMORY_H

#include <linux/compiler.h>
#include <linux/const.h>
#include <linux/types.h>
#include <linux/sizes.h>

#ifndef __ASSEMBLY__
#include <lx_emul/debug.h>
#include <lx_emul/alloc.h>

#define PAGE_OFFSET (0)

#define TASK_SIZE          (UL(CONFIG_PAGE_OFFSET) - UL(SZ_16M))
#define TASK_SIZE_26       (UL(1) << 26)
#define TASK_UNMAPPED_BASE ALIGN(TASK_SIZE / 3, SZ_16M)

#define __va(x) ( lx_emul_trace_and_stop("__va"), (void *)0 )
#define __pa(v) lx_emul_mem_dma_addr((void *)(v))

#define virt_addr_valid(kaddr) (kaddr != NULL)

#define arch_phys_to_idmap_offset 0ULL

static inline unsigned long phys_to_idmap(phys_addr_t addr)
{
	return addr;
}

static inline phys_addr_t idmap_to_phys(unsigned long idmap)
{
	return (phys_addr_t)idmap;
}


#ifndef __virt_to_bus
#define __virt_to_bus    __pa
#define __bus_to_virt    __va
#define __pfn_to_bus(x)  __pfn_to_phys(x)
#define __bus_to_pfn(x)  __phys_to_pfn(x)
#endif

#define virt_to_pfn(kaddr)  (__pa(kaddr) >> PAGE_SHIFT)


#endif /* __ASSEMBLY__ */

#include <asm-generic/memory_model.h>

#endif /* __ASM_MEMORY_H */
