/*
 * \brief  ARM specific implemenations used on all SOCs
 * \author Sebastian Sumpf
 * \date   2016-04-25
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/attached_io_mem_dataspace.h>
#include <base/env.h>

#include <lx_emul.h>

#include <lx_kit/backend_alloc.h>
#include <lx_kit/env.h>
#include <lx_kit/irq.h>


/****************************
 ** lx_kit/backend_alloc.h **
 ****************************/

void backend_alloc_init(Genode::Env&, Genode::Ram_session&,
                        Genode::Allocator&)
{
	/* intentionally left blank */
}


Genode::Ram_dataspace_capability
Lx::backend_alloc(Genode::addr_t size, Genode::Cache_attribute cached) {
	return Lx_kit::env().env().ram().alloc(size, cached); }


void Lx::backend_free(Genode::Ram_dataspace_capability cap) {
	return Lx_kit::env().env().ram().free(cap); }


/***********************
 ** linux/interrupt.h **
 ***********************/

extern "C" int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
                           const char *name, void *dev)
{
	Lx::Irq::irq().request_irq(Platform::Device::create(Lx_kit::env().env(), irq), handler, dev);

	return 0;
}


int devm_request_irq(struct device *dev, unsigned int irq, irq_handler_t handler, unsigned long irqflags, const char *devname, void *dev_id)
{
	Lx::Irq::irq().request_irq(Platform::Device::create(Lx_kit::env().env(), irq), handler, dev_id);
	return 0;
}


/**********************
 ** asm-generic/io.h **
 **********************/

void *_ioremap(phys_addr_t phys_addr, unsigned long size, int wc)
{
	try {
		Genode::Attached_io_mem_dataspace *ds =
			new(Lx::Malloc::mem())
				Genode::Attached_io_mem_dataspace(Lx_kit::env().env(), phys_addr, size, !!wc);
		return ds->local_addr<void>();
	} catch (...) {
		panic("Failed to request I/O memory: [%lx,%lx)", phys_addr, phys_addr + size);
		return 0;
	}
}


void *ioremap(phys_addr_t offset, unsigned long size)
{
	return _ioremap(offset, size, 0);
}


