/*
 * \brief  Replaces kernel/softirq.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/interrupt.h>
#include <linux/bottom_half.h>

#define CREATE_TRACE_POINTS
#include <trace/events/irq.h>

#include <lx_emul/debug.h>

irq_cpustat_t irq_stat;

int __init __weak arch_probe_nr_irqs(void)
{
	return 0;
}

int __init __weak arch_early_irq_init(void)
{
	return 0;
}

unsigned int __weak arch_dynirq_lower_bound(unsigned int from)
{
	return from;
}

static struct softirq_action actions[NR_SOFTIRQS];

void open_softirq(int nr, void (* action)(struct softirq_action *))
{
	if (nr >= NR_SOFTIRQS) {
		printk("Error: %s nr=%d exceeds softirq limit\n", __func__, nr);
		return;
	}

	actions[nr].action = action;
}


inline void raise_softirq_irqoff(unsigned int nr)
{
	if (nr >= NR_SOFTIRQS || !actions[nr].action)
		return;

	actions[nr].action(&actions[nr]);
}


void raise_softirq(unsigned int nr)
{
	raise_softirq_irqoff(nr);
}


void __local_bh_enable_ip(unsigned long ip,unsigned int cnt)
{
	/*
	 * Called by write_unlock_bh, which reverts preempt_cnt by the
	 * value SOFTIRQ_LOCK_OFFSET.
	 */
	__preempt_count_sub(cnt);
}


void __init softirq_init(void) {}


void irq_enter(void) {}


void irq_exit(void) {}
