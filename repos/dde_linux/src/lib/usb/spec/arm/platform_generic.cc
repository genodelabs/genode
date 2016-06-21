/**
 * \brief  ARM specific implemenations used on all SOCs
 * \author Sebastian Sumpf
 * \date   2016-04-25
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <lx_emul/irq.h>
#include <base/env.h>
#include <lx_kit/backend_alloc.h>
#include <lx_kit/env.h>
#include <lx_kit/irq.h>


/****************************
 ** lx_kit/backend_alloc.h **
 ****************************/

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
	Lx::Irq::irq().request_irq(Platform::Device::create(irq), handler, dev);

	return 0;
}
