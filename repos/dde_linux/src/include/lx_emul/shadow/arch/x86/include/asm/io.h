/**
 * \brief  Shadow copy of asm/io.h
 * \author Josef Soentgen
 * \date   2022-02-21
 */

#ifndef _ASM_X86_IO_H
#define _ASM_X86_IO_H

#define ARCH_HAS_IOREMAP_WC
#define ARCH_HAS_IOREMAP_WT

#include <linux/string.h>
#include <linux/compiler.h>
#include <asm/page.h>
#include <asm/early_ioremap.h>
#include <asm/pgtable_types.h>

#include <lx_emul/io_port.h>

#ifndef page_to_phys
#define page_to_phys(page)  ((dma_addr_t)page_to_pfn(page) << PAGE_SHIFT)
#endif

void __iomem *ioremap(resource_size_t offset, unsigned long size);
void __iomem *ioremap_cache(resource_size_t offset, unsigned long size);
void iounmap(volatile void __iomem *addr);

#define inb lx_emul_io_port_inb
#define inw lx_emul_io_port_inw
#define inl lx_emul_io_port_inl

#define outb lx_emul_io_port_outb
#define outw lx_emul_io_port_outw
#define outl lx_emul_io_port_outl

void __iomem *ioremap_wc(resource_size_t offset, unsigned long size);
#define ioremap_wc ioremap_wc

#include <asm-generic/io.h>

#endif /* _ASM_X86_IO_H */
