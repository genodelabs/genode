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
	struct Irq : Genode::List<Irq>::Element {
		unsigned const nr;
		Genode::Irq_connection irq_con { Lx_kit::env().env(), nr };
		Irq(unsigned const irq, Genode::List<Irq> & list)
			: nr(irq) { list.insert(this); }
	};

	static Genode::List<Irq> irq_list;

	Irq * i = irq_list.first();
	for (; i; i = i->next()) { if (i->nr == irq) break; }
	if (!i) i = new (Lx_kit::env().heap()) Irq(irq, irq_list);

	Lx::Irq::irq().request_irq(i->irq_con.cap(), irq, handler, dev);

	return 0;
}
