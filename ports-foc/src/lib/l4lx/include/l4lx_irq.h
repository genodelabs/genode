/*
 * \brief  L4lxapi library IRQ functions
 * \author Stefan Kalkowski
 * \date   2011-04-29
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4LX__L4LX_IRQ_H_
#define _L4LX__L4LX_IRQ_H_

#include <linux.h>

#ifdef __cplusplus
extern "C" {
#endif

struct l4x_irq_desc_private {
	Fiasco::l4_cap_idx_t  irq_cap;
	Fiasco::l4_cap_idx_t  irq_thread;
	unsigned      enabled;
	unsigned      cpu;
	unsigned char trigger;
};

struct irq_data {
	unsigned int     irq;
	unsigned long    hwirq;
	unsigned int     node;
	unsigned int     state_use_accessors;
	struct irq_chip *chip;
	struct irq_domain       *domain;
	void            *handler_data;
	void            *chip_data;
};

/* Linux functions */
FASTCALL int irq_set_chip_data(unsigned int irq, void *data);
FASTCALL struct irq_data *irq_get_irq_data(unsigned int irq);

static inline void *irq_get_chip_data(unsigned int irq)
{
	struct irq_data *d = irq_get_irq_data(irq);
	return d ? d->chip_data : 0;
}

/* l4lxapi functions */
FASTCALL void l4lx_irq_init(void);
FASTCALL int l4lx_irq_prio_get(unsigned int irq);
FASTCALL unsigned int l4lx_irq_dev_startup(struct irq_data *data);
FASTCALL void l4lx_irq_dev_shutdown(struct irq_data *data);
FASTCALL int l4lx_irq_set_type(struct irq_data *data, unsigned int type);
FASTCALL void l4lx_irq_dev_enable(struct irq_data *data);
FASTCALL void l4lx_irq_dev_disable(struct irq_data *data);
FASTCALL void l4lx_irq_dev_ack(struct irq_data *data);
FASTCALL void l4lx_irq_dev_mask(struct irq_data *data);
FASTCALL void l4lx_irq_dev_unmask(struct irq_data *data);
FASTCALL int l4lx_irq_dev_set_affinity(struct irq_data *data,
                                       const struct cpumask *dest, bool force);
FASTCALL void l4lx_irq_dev_eoi(struct irq_data *data);
FASTCALL unsigned int l4lx_irq_timer_startup(struct irq_data *data);
FASTCALL void l4lx_irq_timer_shutdown(struct irq_data *data);
FASTCALL void l4lx_irq_timer_enable(struct irq_data *data);
FASTCALL void l4lx_irq_timer_disable(struct irq_data *data);
FASTCALL void l4lx_irq_timer_ack(struct irq_data *data);
FASTCALL void l4lx_irq_timer_mask(struct irq_data *data);
FASTCALL void l4lx_irq_timer_unmask(struct irq_data *data);
FASTCALL int l4lx_irq_timer_set_affinity(struct irq_data *data,
                                         const struct cpumask *dest);
FASTCALL void l4lx_irq_dev_enable(struct irq_data *data);
FASTCALL int l4x_alloc_irq_desc_data(int irq);

#ifdef __cplusplus
}
#endif

#endif /* _L4LX__L4LX_IRQ_H_ */
