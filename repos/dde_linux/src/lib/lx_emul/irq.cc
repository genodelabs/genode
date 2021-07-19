/*
 * \brief  Lx_emul backend for interrupts
 * \author Stefan Kalkowski
 * \date   2021-04-22
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul/irq.h>
#include <lx_kit/env.h>

extern "C" void lx_emul_irq_unmask(unsigned int irq)
{
	bool found = false;
	Lx_kit::env().devices.for_each([&] (Lx_kit::Device & d) {
		if (d.irq_unmask(irq)) found = true; });

	if (!found)
		Genode::error("irq ", irq, " unavailable");
}


extern "C" void lx_emul_irq_mask(unsigned int irq)
{
	Lx_kit::env().devices.for_each([&] (Lx_kit::Device & d) {
		d.irq_mask(irq); });
}


extern "C" void lx_emul_irq_eoi(unsigned int irq)
{
	Lx_kit::env().devices.for_each([&] (Lx_kit::Device & d) {
		d.irq_ack(irq); });
}


extern "C" unsigned int lx_emul_irq_last()
{
	return Lx_kit::env().last_irq;
}
