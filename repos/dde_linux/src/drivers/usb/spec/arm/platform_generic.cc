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

#include <base/env.h>
#include <irq_session/connection.h>

#include <lx_emul/irq.h>
#include <lx_kit/backend_alloc.h>
#include <lx_kit/env.h>
#include <lx_kit/irq.h>


/****************************
 ** lx_kit/backend_alloc.h **
 ****************************/

void backend_alloc_init(Genode::Env&, Genode::Ram_allocator&,
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
	Genode::Irq_connection * irq_con = new (Lx_kit::env().heap())
		Genode::Irq_connection(Lx_kit::env().env(), irq);
	Lx::Irq::irq().request_irq(irq_con->cap(), irq, handler, dev);

	return 0;
}
