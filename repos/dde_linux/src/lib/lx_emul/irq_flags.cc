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

static unsigned long cpu_local_irq_flags = 0;


extern "C" unsigned long lx_emul_irq_enable(void)
{
	unsigned long ret = cpu_local_irq_flags;
	cpu_local_irq_flags = 0;
	return ret;
}


extern "C" unsigned long lx_emul_irq_disable(void)
{
	unsigned long ret = cpu_local_irq_flags;
	cpu_local_irq_flags = 1;
	return ret;
}


extern "C" unsigned long lx_emul_irq_state(void)
{
	return cpu_local_irq_flags;
}


extern "C" void lx_emul_irq_restore(unsigned long flags)
{
	cpu_local_irq_flags = flags;
}

