/*
 * \brief  Replaces lib/logic_iomem.c
 * \author Josef Soentgen
 * \date   2022-04-05
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#include <lx_emul/io_mem.h>

#include <asm-generic/logic_io.h>


void __iomem * ioremap(resource_size_t phys_addr, unsigned long size)
{
    return lx_emul_io_mem_map(phys_addr, size);
}


void iounmap(volatile void __iomem * addr)
{
    (void)addr;
}
