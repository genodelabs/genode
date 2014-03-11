/*
 * \brief   A multiplexable common instruction processor
 * \author  Martin Stein
 * \date    2014-01-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/processor.h>
#include <kernel/irq.h>
#include <pic.h>
#include <timer.h>

using namespace Kernel;

namespace Kernel
{
	Pic * pic();
	Timer * timer();
}


void Kernel::Processor_client::_interrupt(unsigned const processor_id)
{
	/* determine handling for specific interrupt */
	unsigned irq_id;
	Pic * const ic = pic();
	if (ic->take_request(irq_id))
	{
		/* check wether the interrupt is a processor-scheduling timeout */
		if (timer()->interrupt_id(processor_id) == irq_id) {

			__processor->scheduler()->yield_occupation();
			timer()->clear_interrupt(processor_id);

		/* check wether the interrupt is our inter-processor interrupt */
		} else if (ic->is_ip_interrupt(irq_id, processor_id)) {

			__processor->ip_interrupt();

		/* after all it must be a user interrupt */
		} else {

			/* try to inform the user interrupt-handler */
			Irq::occurred(irq_id);
		}
	}
	/* end interrupt request at controller */
	ic->finish_request();
}


void Kernel::Processor_client::_schedule() { __processor->schedule(this); }


void Kernel::Processor::schedule(Processor_client * const client)
{
	if (_id != executing_id()) {

		/*
		 * Remote add client and let target processor notice it if necessary
		 *
		 * The interrupt controller might provide redundant submission of
		 * inter-processor interrupts. Thus its possible that once the targeted
		 * processor is able to grab the kernel lock, multiple remote updates
		 * occured and consequently the processor traps multiple times for the
		 * sole purpose of recognizing the result of the accumulative changes.
		 * Hence, we omit further interrupts if there is one pending already.
		 * Additionailly we omit the interrupt if the insertion doesn't
		 * rescind the current scheduling choice of the processor.
		 */
		if (_scheduler.insert_and_check(client) && !_ip_interrupt_pending) {
			pic()->trigger_ip_interrupt(_id);
			_ip_interrupt_pending = true;
		}
	} else {

		/* add client locally */
		_scheduler.insert(client);
	}
}


void Kernel::Processor_client::_unschedule()
{
	assert(__processor->id() == Processor::executing_id());
	__processor->scheduler()->remove(this);
}


void Kernel::Processor_client::_yield()
{
	assert(__processor->id() == Processor::executing_id());
	__processor->scheduler()->yield_occupation();
}
