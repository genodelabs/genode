/*
 * \brief  Kernel function implementations specific for Cortex A9 CPUs
 * \author Stefan Kalkowski
 * \date   2012-10-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <kernel_support.h>


Cpu::User_context::User_context()
{
	/* Execute in usermode with IRQ's enabled and FIQ's and
	 * asynchronous aborts disabled */
	cpsr = Cpsr::M::bits(Cpsr::M::USER) | Cpsr::F::bits(1) |
		Cpsr::I::bits(0) | Cpsr::A::bits(1);
}
