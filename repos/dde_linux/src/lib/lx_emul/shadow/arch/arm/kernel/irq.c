/*
 * \brief  Replaces arch/arm/kernel/irq.c
 * \author Josef Soentgen
 * \date   2026-01-19
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <linux/interrupt.h>


void do_softirq_own_stack(void)
{
	/*
	 * We have no IRQ stack to switch to anyway,
	 * so we stay here in contrast to the original
	 * implementation
	 */
	__do_softirq();
}
