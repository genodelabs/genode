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

extern "C" {
#include <dde_kit/pci.h>
}

#include "pci_tree.h"

static const bool verbose = false;


static Dde_kit::Pci_tree *pci_tree()
{
	static Dde_kit::Pci_tree _pci_tree;

	return &_pci_tree;
}


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


/********************
 ** Initialization **
 ********************/

extern "C" void dde_kit_pci_init(void)
{
	try {
		pci_tree();
	} catch (...) {
		PERR("PCI initialization failed");
	}
}
