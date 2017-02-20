/*
 * \brief  Implementation of linux/io.h
 * \author Norman Feske
 * \date   2015-09-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Linux kit includes */
#include <lx_kit/pci_dev_registry.h>
#include <lx_kit/mapped_io_mem_range.h>


void *ioremap(phys_addr_t phys_addr, unsigned long size)
{
	return Lx::ioremap(phys_addr, size, Genode::UNCACHED);
}


void *ioremap_wc(resource_size_t phys_addr, unsigned long size)
{
	return Lx::ioremap(phys_addr, size, Genode::WRITE_COMBINED);
}


/**********************
 ** asm-generic/io.h **
 **********************/

void outb(u8  value, u32 port) { Lx::pci_dev_registry()->io_write<u8> (port, value); }
void outw(u16 value, u32 port) { Lx::pci_dev_registry()->io_write<u16>(port, value); }
void outl(u32 value, u32 port) { Lx::pci_dev_registry()->io_write<u32>(port, value); }

u8  inb(u32 port) { return Lx::pci_dev_registry()->io_read<u8> (port); }
u16 inw(u32 port) { return Lx::pci_dev_registry()->io_read<u16>(port); }
u32 inl(u32 port) { return Lx::pci_dev_registry()->io_read<u32>(port); }
