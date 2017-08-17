/**
 * \brief  DRM library dummies
 * \author Sebastian Sumpf
 * \date   2017-08-17
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */



extern "C" {
#include <pciaccess.h>
}

#include <base/log.h>

#define TRACE Genode::log(__func__, "called  not implemented");

int pci_system_init(void)
{
	TRACE;
	return -1;
}


void pci_system_cleanup(void)
{
	TRACE;
}


struct pci_device *pci_device_find_by_slot(uint32_t domain, uint32_t bus,
                                           uint32_t dev, uint32_t func)
{
	TRACE;
	return nullptr;
}


int pci_device_probe(struct pci_device *dev)
{
	TRACE;
	return -1;
}

extern "C" int posix_memalign(void **memptr, size_t alignment, size_t size)
{
	TRACE;
	return -1;
}
