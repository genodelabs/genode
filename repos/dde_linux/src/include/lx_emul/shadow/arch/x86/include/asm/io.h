/**
 * \brief  Shadow copy of asm/io.h
 * \author Josef Soentgen
 * \date   2022-02-21
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
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

#define build_mmio_read(name, size, type, reg, barrier) \
static inline type name(const volatile void __iomem *addr) \
{ type ret; asm volatile("mov" size " %1,%0":reg (ret) \
:"m" (*(volatile type __force *)addr) barrier); return ret; }

#define build_mmio_write(name, size, type, reg, barrier) \
static inline void name(type val, volatile void __iomem *addr) \
{ asm volatile("mov" size " %0,%1": :reg (val), \
"m" (*(volatile type __force *)addr) barrier); }

build_mmio_read(readb, "b", unsigned char, "=q", :"memory")
build_mmio_read(readw, "w", unsigned short, "=r", :"memory")
build_mmio_read(readl, "l", unsigned int, "=r", :"memory")

build_mmio_read(__readb, "b", unsigned char, "=q", )
build_mmio_read(__readw, "w", unsigned short, "=r", )
build_mmio_read(__readl, "l", unsigned int, "=r", )

build_mmio_write(writeb, "b", unsigned char, "q", :"memory")
build_mmio_write(writew, "w", unsigned short, "r", :"memory")
build_mmio_write(writel, "l", unsigned int, "r", :"memory")

build_mmio_write(__writeb, "b", unsigned char, "q", )
build_mmio_write(__writew, "w", unsigned short, "r", )
build_mmio_write(__writel, "l", unsigned int, "r", )

#define readb readb
#define readw readw
#define readl readl
#define readb_relaxed(a) __readb(a)
#define readw_relaxed(a) __readw(a)
#define readl_relaxed(a) __readl(a)
#define __raw_readb __readb
#define __raw_readw __readw
#define __raw_readl __readl

#define writeb writeb
#define writew writew
#define writel writel
#define writeb_relaxed(v, a) __writeb(v, a)
#define writew_relaxed(v, a) __writew(v, a)
#define writel_relaxed(v, a) __writel(v, a)
#define __raw_writeb __writeb
#define __raw_writew __writew
#define __raw_writel __writel

#ifdef CONFIG_X86_64

build_mmio_read(readq, "q", u64, "=r", :"memory")
build_mmio_read(__readq, "q", u64, "=r", )
build_mmio_write(writeq, "q", u64, "r", :"memory")
build_mmio_write(__writeq, "q", u64, "r", )

#define readq_relaxed(a)        __readq(a)
#define writeq_relaxed(v, a)    __writeq(v, a)

#define __raw_readq             __readq
#define __raw_writeq            __writeq

/* Let people know that we have them */
#define readq                   readq
#define writeq                  writeq

#endif

#include <asm-generic/io.h>

#endif /* _ASM_X86_IO_H */
