/*
 * \brief  PCI bus access
 * \author Christian Helmuth
 * \date   2008-10-22
 *
 * We provide a virtual PCI bus hierarchy in pci_tree.cc:
 *
 * - Grab all accessible devices at pci_drv and populate virtual bus hierarchy.
 *   (This also works if parents/device managers limit device access in the
 *   future.)
 * - DDE kit C interface in pci.cc
 *   - Read/write config space
 *   - Convenience functions
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <dataspace/client.h>

#include <io_port_session/capability.h>

extern "C" {
#include <dde_kit/pci.h>
#include <dde_kit/pgtab.h>
}

#include "pci_tree.h"
#include "device.h"

static const bool verbose = false;


static Dde_kit::Pci_tree *pci_tree(unsigned device_class = 0,
                                   unsigned class_mask = 0)
{
	static Dde_kit::Pci_tree _pci_tree(device_class, class_mask);

	return &_pci_tree;
}


Genode::Io_port_session_capability Dde_kit::Device::io_port(int bus, int dev, int fun, unsigned short bda) {
	return pci_tree()->io_port(bus, dev, fun, bda); }


/********************************
 ** Configuration space access **
 ********************************/

extern "C" void dde_kit_pci_readb(int bus, int dev, int fun, int pos, dde_kit_uint8_t *val)
{
	try {
		*val = pci_tree()->config_read(bus, dev, fun, pos, Pci::Device::ACCESS_8BIT) & 0xff;
	} catch (...) {
		if (verbose)
			PWRN("PCI device %02x:%02x.%x not found", bus, dev, fun);
		*val = ~0;
	}
}


extern "C" void dde_kit_pci_readw(int bus, int dev, int fun, int pos, dde_kit_uint16_t *val)
{
	try {
		*val = pci_tree()->config_read(bus, dev, fun, pos, Pci::Device::ACCESS_16BIT) & 0xffff;
	} catch (...) {
		if (verbose)
			PWRN("PCI device %02x:%02x.%x not found", bus, dev, fun);
		*val = ~0;
	}
}


extern "C" void dde_kit_pci_readl(int bus, int dev, int fun, int pos, dde_kit_uint32_t *val)
{
	try {
		*val = pci_tree()->config_read(bus, dev, fun, pos, Pci::Device::ACCESS_32BIT);
	} catch (...) {
		if (verbose)
			PWRN("PCI device %02x:%02x.%x not found", bus, dev, fun);
		*val = ~0;
	}
}


extern "C" void dde_kit_pci_writeb(int bus, int dev, int fun, int pos, dde_kit_uint8_t val)
{
	try {
		pci_tree()->config_write(bus, dev, fun, pos, val, Pci::Device::ACCESS_8BIT);
	} catch (...) {
		if (verbose)
			PWRN("PCI device %02x:%02x.%x not found", bus, dev, fun);
	}
}

extern "C" void dde_kit_pci_writew(int bus, int dev, int fun, int pos, dde_kit_uint16_t val)
{
	try {
		pci_tree()->config_write(bus, dev, fun, pos, val, Pci::Device::ACCESS_16BIT);
	} catch (...) {
		if (verbose)
			PWRN("PCI device %02x:%02x.%x not found", bus, dev, fun);
	}
}


extern "C" void dde_kit_pci_writel(int bus, int dev, int fun, int pos, dde_kit_uint32_t val)
{
	try {
		pci_tree()->config_write(bus, dev, fun, pos, val, Pci::Device::ACCESS_32BIT);
	} catch (...) {
		if (verbose)
			PWRN("PCI device %02x:%02x.%x not found", bus, dev, fun);
	}
}


/***************************
 ** Convenience functions **
 ***************************/

extern "C" int dde_kit_pci_first_device(int *bus, int *dev, int *fun)
{
	try {
		pci_tree()->first_device(bus, dev, fun);
		return 0;
	} catch (...) {
		return -1;
	}
}


extern "C" int dde_kit_pci_next_device(int *bus, int *dev, int *fun)
{
	try {
		pci_tree()->next_device(bus, dev, fun);
		return 0;
	} catch (...) {
		return -1;
	}
}

extern "C" dde_kit_addr_t dde_kit_pci_alloc_dma_buffer(int bus, int dev,
                                                       int fun, 
                                                       dde_kit_size_t size)
{
	try {
		using namespace Genode;

		Ram_dataspace_capability ram_cap;
		ram_cap = pci_tree()->alloc_dma_buffer(bus, dev, fun, size);
		addr_t base = (addr_t)env()->rm_session()->attach(ram_cap);

		/* add to DDE-kit page tables */
		addr_t phys = Dataspace_client(ram_cap).phys_addr();
		dde_kit_pgtab_set_region_with_size((void *)base, phys, size);

		return base;
	} catch (...) {
		return 0;
	}
}

/********************
 ** Initialization **
 ********************/

extern "C" void dde_kit_pci_init(unsigned device_class, unsigned class_mask)
{
	try {
		pci_tree(device_class, class_mask);
	} catch (...) {
		PERR("PCI initialization failed");
	}
}
