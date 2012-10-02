/*
 * \brief  Kernel-specific implementations for Versatile Express with TZ
 * \author Stefan Kalkowski
 * \date   2012-10-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Core includes */
#include <kernel_support.h>


Genode::Cortex_a9::User_context::User_context()
{
	/* Execute in usermode with FIQ's enabled and IRQ's and
	 * asynchronous aborts disabled */
	cpsr = Cpsr::M::bits(Cpsr::M::USER) | Cpsr::F::bits(0) |
	       Cpsr::I::bits(1) | Cpsr::A::bits(1);
}
