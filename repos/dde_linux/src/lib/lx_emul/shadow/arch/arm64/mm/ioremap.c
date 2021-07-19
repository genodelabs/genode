/*
 * \brief  Replaces arch/arm64/mm/ioremap.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <asm/io.h>
#include <lx_emul/io_mem.h>

void __iomem * __ioremap(phys_addr_t phys_addr, size_t size, pgprot_t prot)
{
	return lx_emul_io_mem_map(phys_addr, size);
}


void iounmap(volatile void __iomem * io_addr) { }


