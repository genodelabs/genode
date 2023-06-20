/*
 * \brief  Linux DDE x86 interrupt controller
 * \author Josef Soentgen
 * \date   2022-01-20
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul/debug.h>
#include <lx_emul/irq.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <../kernel/irq/internals.h>


static void dde_irq_unmask(struct irq_data *d)
{
	lx_emul_irq_unmask(d->hwirq);
}


static void dde_irq_mask(struct irq_data *d)
{
	lx_emul_irq_mask(d->hwirq);
}


int lx_emul_irq_init(struct device_node *node, struct device_node *parent)
{
	return 0;
}


struct irq_chip dde_irqchip_data_chip = {
	.name           = "dde-irqs",
	.irq_mask       = dde_irq_mask,
	.irq_disable    = dde_irq_mask,
	.irq_unmask     = dde_irq_unmask,
	.irq_mask_ack   = dde_irq_mask,
};


int lx_emul_irq_task_function(void * data)
{
	unsigned long flags;
	int irq;

	for (;;) {
		lx_emul_task_schedule(true);

		for (;;) {

			irq = lx_emul_pending_irq();
			if (irq == -1)
				break;

			local_irq_save(flags);
			irq_enter();

			if (!irq) {
				ack_bad_irq(irq);
				WARN_ONCE(true, "Unexpected interrupt %d received!\n",
				          irq);
			} else {
				generic_handle_irq(irq);
				lx_emul_irq_eoi(irq);
			}

			irq_exit();
			local_irq_restore(flags);
		}
	}

	return 0;
}


struct task_struct irq_task = {
	.__state         = 0,
	.usage           = REFCOUNT_INIT(2),
	.flags           = PF_KTHREAD,
	.prio            = MAX_PRIO - 20,
	.static_prio     = MAX_PRIO - 20,
	.normal_prio     = MAX_PRIO - 20,
	.policy          = SCHED_NORMAL,
	.cpus_ptr        = &irq_task.cpus_mask,
	.cpus_mask       = CPU_MASK_ALL,
	.nr_cpus_allowed = 1,
	.mm              = NULL,
	.active_mm       = NULL,
	.tasks           = LIST_HEAD_INIT(irq_task.tasks),
	.real_parent     = &irq_task,
	.parent          = &irq_task,
	.children        = LIST_HEAD_INIT(irq_task.children),
	.sibling         = LIST_HEAD_INIT(irq_task.sibling),
	.group_leader    = &irq_task,
	.comm            = "kirqd",
	.thread          = INIT_THREAD,
	.pending         = {
		.list   = LIST_HEAD_INIT(irq_task.pending.list),
		.signal = {{0}}
	},
	.blocked         = {{0}},
};
void * lx_emul_irq_task_struct = &irq_task;
