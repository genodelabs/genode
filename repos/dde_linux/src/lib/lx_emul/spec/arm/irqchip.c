/*
 * \brief  Linux DDE interrupt controller
 * \author Stefan Kalkowski
 * \date   2021-03-10
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul/debug.h>
#include <lx_emul/irq.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/irqchip.h>
#include <../kernel/irq/internals.h>

static int dde_irq_set_wake(struct irq_data *d, unsigned int on)
{
	lx_emul_trace_and_stop(__func__);
	return 0;
}


static void dde_irq_unmask(struct irq_data *d)
{
	lx_emul_irq_eoi(d->hwirq);
	lx_emul_irq_unmask(d->hwirq);
}


static void dde_irq_mask(struct irq_data *d)
{
	lx_emul_irq_mask(d->hwirq);
}


static void dde_irq_eoi(struct irq_data *d)
{
	lx_emul_irq_eoi(d->hwirq);
}


static int dde_irq_set_type(struct irq_data *d, unsigned int type)
{
	if (type != IRQ_TYPE_LEVEL_HIGH)
		lx_emul_trace_and_stop(__func__);
	return 0;
}


struct irq_chip dde_irqchip_data_chip = {
	.name           = "dde-irqs",
	.irq_eoi        = dde_irq_eoi,
	.irq_mask       = dde_irq_mask,
	.irq_unmask     = dde_irq_unmask,
	.irq_set_wake   = dde_irq_set_wake,
	.irq_set_type   = dde_irq_set_type,
};


static int dde_domain_translate(struct irq_domain * d,
                                  struct irq_fwspec * fwspec,
                                  unsigned long     * hwirq,
                                  unsigned int      * type)
{
	if (fwspec->param_count == 1) {
		*hwirq = fwspec->param[0];
		*type  = 0;
		return 0;
	}

	if (is_of_node(fwspec->fwnode)) {
		if (fwspec->param_count != 3 || fwspec->param[0] != 0)
			return -EINVAL;

		*hwirq = fwspec->param[1] + 32;
		*type  = fwspec->param[2] & IRQ_TYPE_SENSE_MASK;
		return 0;
	}

	return -EINVAL;
}


static int dde_domain_alloc(struct irq_domain * domain,
                              unsigned int        irq,
                              unsigned int        nr_irqs,
                              void              * data)
{
	struct irq_fwspec *fwspec = data;
	irq_hw_number_t hwirq;
	unsigned int type;
	int err;
	int i;

	err = dde_domain_translate(domain, fwspec, &hwirq, &type);
	if (err)
		return err;

	for (i = 0; i < nr_irqs; i++) {
		irq_domain_set_info(domain, irq+i, hwirq+i, &dde_irqchip_data_chip,
		                    domain->host_data, handle_fasteoi_irq, NULL, NULL);
		irq_set_probe(irq);
		irqd_set_single_target(irq_desc_get_irq_data(irq_to_desc(irq)));
	}

	return 0;
}


static const struct irq_domain_ops dde_irqchip_data_domain_ops = {
	.translate	= dde_domain_translate,
	.alloc		= dde_domain_alloc,
	.free		= irq_domain_free_irqs_common,
};


static struct irq_domain *dde_irq_domain;

int lx_emul_irq_init(struct device_node *node, struct device_node *parent)
{
	dde_irq_domain = irq_domain_create_tree(&node->fwnode,
	                                        &dde_irqchip_data_domain_ops, NULL);
	if (!dde_irq_domain)
		return -ENOMEM;

	irq_set_default_host(dde_irq_domain);
	return 0;
}


enum { LX_EMUL_MAX_OF_IRQ_CHIPS = 16 };


struct of_device_id __irqchip_of_table[LX_EMUL_MAX_OF_IRQ_CHIPS] = { };


void lx_emul_register_of_irqchip_initcall(char const *compat, void *fn)
{
	static unsigned count;

	if (count == LX_EMUL_MAX_OF_IRQ_CHIPS) {
		printk("lx_emul_register_of_irqchip_initcall: __irqchip_of_table exhausted\n");
		return;
	}

	strncpy(__irqchip_of_table[count].compatible, compat,
	        sizeof(__irqchip_of_table[count].compatible));

	__irqchip_of_table[count].data = fn;

	count++;
}


IRQCHIP_DECLARE(dde_gic_v3,  "arm,gic-v3",        lx_emul_irq_init);
IRQCHIP_DECLARE(dde_gic_a9,  "arm,cortex-a9-gic", lx_emul_irq_init);
IRQCHIP_DECLARE(dde_gic_400, "arm,gic-400",       lx_emul_irq_init);


int lx_emul_irq_task_function(void * data)
{
	int irq;
	unsigned long flags;

	for (;;) {
		lx_emul_task_schedule(true);

		for (;;) {
			irq = lx_emul_pending_irq();
			if (irq == -1)
				break;

			local_irq_save(flags);
			irq_enter();

			irq = dde_irq_domain ? irq_find_mapping(dde_irq_domain,
			                                        irq)
			                     : irq;

			if (!irq) {
				ack_bad_irq(irq);
				WARN_ONCE(true, "Unexpected interrupt %d received!\n", irq);
			} else {
				generic_handle_irq(irq);
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
