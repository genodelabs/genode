/*
 * \brief  Replaces arch/x86/kernel/irq_32.c
 * \author Stefan Kalkowski
 * \date   2022-07-20
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
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
