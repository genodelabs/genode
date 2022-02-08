/*
 * \brief  Shadows Linux kernel arch/.../asm/memory.h
 * \author Norman Feske
 * \date   2021-06-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef __ASM_MEMORY_H
#define __ASM_MEMORY_H

#ifndef __ASSEMBLY__

#include <asm/page-def.h>
#include <linux/sizes.h>
#include <lx_emul/debug.h>
#include <lx_emul/alloc.h>
#include <lx_emul/page_virt.h>

#define PCI_IO_SIZE  SZ_16M

extern u64 vabits_actual;

#define __tag_reset(addr)   (addr)
#define untagged_addr(addr) (addr)

#define VA_BITS     (CONFIG_ARM64_VA_BITS)
#define VA_BITS_MIN (VA_BITS)

#define PAGE_OFFSET (0)

#define THREAD_SHIFT PAGE_SHIFT
#define THREAD_SIZE  (UL(1) << THREAD_SHIFT)

#define BPF_JIT_REGION_START 0
#define BPF_JIT_REGION_END   0

#define MT_NORMAL        0
#define MT_NORMAL_TAGGED 1
#define MT_NORMAL_NC     2
#define MT_DEVICE_nGnRnE 4
#define MT_DEVICE_nGnRE  5

#define __va(x) ( lx_emul_trace_and_stop("__va"), (void *)0 )
#define __pa(v) lx_emul_mem_dma_addr((void *)(v))

#define page_to_phys(p) __pa((p)->virtual)
#define page_to_virt(p)     ((p)->virtual)

static inline struct page *virt_to_page(void const *v) { return lx_emul_virt_to_pages(v, 1U); }

#define pfn_to_page(pfn) ( (struct page *)(__va(pfn << PAGE_SHIFT)) )
#define page_to_pfn(page) ( page_to_phys(page) >> PAGE_SHIFT )

#define PCI_IO_START 0

#endif /* __ASSEMBLY__ */

#endif /* __ASM_MEMORY_H */
