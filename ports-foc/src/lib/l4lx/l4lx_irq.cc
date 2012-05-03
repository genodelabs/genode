/*
 * \brief  L4lxapi library IRQ functions.
 * \author Stefan Kalkowski
 * \date   2011-04-11
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/snprintf.h>

#include <env.h>
#include <l4lx_irq.h>
#include <l4lx_thread.h>
#include <vcpu.h>

namespace Fiasco {
#include <l4/sys/types.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/kip.h>
#include <l4/sys/vcpu.h>
#include <l4/sys/irq.h>
}

using namespace Fiasco;

static const bool DEBUG = false;

enum { TIMER_IRQ = 0 };

unsigned int l4lx_irq_max = l4x_nr_irqs();

extern l4_kernel_info_t *l4lx_kinfo;

void timer_irq_thread(void *data)
{
	l4_timeout_t                 to;
	l4_kernel_clock_t            pint;
	l4_utcb_t                   *u = l4_utcb();
	struct l4x_irq_desc_private *p =
		(struct l4x_irq_desc_private*) irq_get_chip_data(TIMER_IRQ);

	pint = l4lx_kinfo->clock;
	for (;;) {
		pint += 1000000 / l4x_hz();

		if (pint > l4lx_kinfo->clock) {
			l4_rcv_timeout(l4_timeout_abs_u(pint, 1, u), &to);
			l4_ipc_receive(L4_INVALID_CAP, u, to);
		}

		if (l4_error(l4_irq_trigger(p->irq_cap)) != -1)
			PWRN("IRQ timer trigger failed\n");
	}
}


static unsigned int startup_timer(struct l4x_irq_desc_private *p)
{
	char          name[15];
	int           cpu = 0;//smp_processor_id();

	unsigned long flags = 0;
	l4x_irq_save(flags);

	Genode::snprintf(name, sizeof(name), "timer.i%d", TIMER_IRQ);

	static Genode::Native_capability timer_cap = L4lx::vcpu_connection()->alloc_irq();
	p->irq_cap = timer_cap.dst();
	p->cpu     = 0;

	l4lx_thread_create(timer_irq_thread, cpu, 0, 0, 0, 0, 0, name);
	l4x_irq_save(flags);

	l4lx_irq_dev_enable(irq_get_irq_data(TIMER_IRQ));
	return 1;
}


extern "C" {

FASTCALL l4_cap_idx_t l4x_have_irqcap(int irqnum);


void l4lx_irq_init(void) { }


int l4lx_irq_prio_get(unsigned int irq)
{
	NOT_IMPLEMENTED;
	return 0;
}


unsigned int l4lx_irq_dev_startup(struct irq_data *data)
{
	unsigned irq = data->irq;
    struct l4x_irq_desc_private *p =
		(struct l4x_irq_desc_private*) irq_get_chip_data(irq);

	if (DEBUG)
		PDBG("irq=%d", irq);

	if (irq == TIMER_IRQ)
		return startup_timer(p);

	/* First test whether a capability has been registered with
	 * this IRQ number */
	p->irq_cap = l4x_have_irqcap(irq);
	if (l4_is_invalid_cap(p->irq_cap)) {
		PERR("Invalid irq cap!");
		return 0;
	}
	l4lx_irq_dev_enable(data);
	return 1;
}


void l4lx_irq_dev_shutdown(struct irq_data *data)
{

	if (data->irq == TIMER_IRQ) {
		PWRN("timer shutdown not implemented yet");
		return;
	}

	if (DEBUG)
		PDBG("irq=%d", data->irq);

	l4lx_irq_dev_disable(data);
}


int l4lx_irq_set_type(struct irq_data *data, unsigned int type)
{
	NOT_IMPLEMENTED;
	return 0;
}


void l4lx_irq_dev_enable(struct irq_data *data)
{
	struct l4x_irq_desc_private *p =
		(struct l4x_irq_desc_private*) irq_get_chip_data(data->irq);;
	unsigned long flags = 0;
	p->enabled = 1;

	if (DEBUG)
		PDBG("irq=%d cap=%lx", data->irq, p->irq_cap);

	l4x_irq_save(flags);
	l4_msgtag_t ret = l4_irq_attach(p->irq_cap, data->irq << 2,
	                                l4x_cpu_thread_get_cap(p->cpu));
	if (l4_error(ret))
		PWRN("Attach to irq %lx failed with error %ld!", p->irq_cap, l4_error(ret));
	l4x_irq_restore(flags);

	l4lx_irq_dev_eoi(data);
}


void l4lx_irq_dev_disable(struct irq_data *data)
{
	struct l4x_irq_desc_private *p =
		(struct l4x_irq_desc_private*) irq_get_chip_data(data->irq);;
	p->enabled = 0;

	if (DEBUG)
		PDBG("irq=%d cap=%lx", data->irq, p->irq_cap);

	Linux::Irq_guard guard;
	if (l4_error(l4_irq_detach(p->irq_cap)))
		PWRN("%02d: Unable to detach from IRQ\n", data->irq);
}


void l4lx_irq_dev_ack(struct irq_data *data) { }


void l4lx_irq_dev_mask(struct irq_data *data)
{
	NOT_IMPLEMENTED;
}


void l4lx_irq_dev_unmask(struct irq_data *data)
{
	NOT_IMPLEMENTED;
}


int l4lx_irq_dev_set_affinity(struct irq_data *data,
                        const struct cpumask *dest, bool force)
{
	NOT_IMPLEMENTED;
	return 0;
}


void l4lx_irq_dev_eoi(struct irq_data *data)
{
	struct l4x_irq_desc_private *p =
		(struct l4x_irq_desc_private*) irq_get_chip_data(data->irq);;

	Linux::Irq_guard guard;
	l4_irq_unmask(p->irq_cap);
}


unsigned int l4lx_irq_timer_startup(struct irq_data *data)
{
	NOT_IMPLEMENTED;
	return 0;
}


void l4lx_irq_timer_shutdown(struct irq_data *data)
{
	NOT_IMPLEMENTED;
}


void l4lx_irq_timer_enable(struct irq_data *data)
{
	NOT_IMPLEMENTED;
}


void l4lx_irq_timer_disable(struct irq_data *data)
{
	NOT_IMPLEMENTED;
}


void l4lx_irq_timer_ack(struct irq_data *data)
{
	NOT_IMPLEMENTED;
}


void l4lx_irq_timer_mask(struct irq_data *data)
{
	NOT_IMPLEMENTED;
}


void l4lx_irq_timer_unmask(struct irq_data *data)
{
	NOT_IMPLEMENTED;
}


int l4lx_irq_timer_set_affinity(struct irq_data *data, const struct cpumask *dest)
{
	NOT_IMPLEMENTED;
	return 0;
}


int l4x_alloc_irq_desc_data(int irq)
{
	struct l4x_irq_desc_private *p;
	Genode::env()->heap()->alloc(sizeof(struct l4x_irq_desc_private), (void**)&p);
	if (!p) {
		PWRN("Could not allocate irq descriptor memory!");
		return -12; //ENOMEM;
	}

	return irq_set_chip_data(irq, p);
}

}
