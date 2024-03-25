/*
 * \brief  Lx_emul support for interrupts
 * \author Stefan Kalkowski
 * \date   2021-04-14
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__IRQ_H_
#define _LX_EMUL__IRQ_H_

#ifdef __cplusplus
extern "C" {
#endif

struct irq_desc;
struct device_node;

void lx_emul_irq_unmask(unsigned int irq);

void lx_emul_irq_mask(unsigned int irq);

void lx_emul_irq_ack(unsigned int irq);

int lx_emul_irq_task_function(void * data);

extern void * lx_emul_irq_task_struct;

int lx_emul_pending_irq(void);

int lx_emul_irq_init(struct device_node *node, struct device_node *parent);

unsigned long lx_emul_irq_enable(void);
unsigned long lx_emul_irq_disable(void);
unsigned long lx_emul_irq_state(void);
void          lx_emul_irq_restore(unsigned long);

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__IRQ_H_ */
