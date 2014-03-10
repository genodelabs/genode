/*
 * \brief   Round-robin scheduler
 * \author  Martin Stein
 * \date    2014-02-28
 */

/*
 * Copyright (C) 2012-2014 Genode Labs GmbH
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


void Kernel::Execution_context::_interrupt(unsigned const processor_id)
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

			/*
			 * This interrupt solely denotes that another processor has
			 * modified the scheduling plan of this processor and thus
			 * a more prior user context than the current one might be
			 * available.
			 */

		/* after all it must be a user interrupt */
		} else {

			/* try to inform the user interrupt-handler */
			Irq::occurred(irq_id);
		}
	}
	/* end interrupt request at controller */
	ic->finish_request();
}


void Kernel::Execution_context::_schedule()
{
	/* schedule thread */
	__processor->scheduler()->insert(this);

	/* let processor of the scheduled thread notice the change immediately */
	unsigned const processor_id = __processor->id();
	if (processor_id != Processor::executing_id()) {
		pic()->trigger_ip_interrupt(processor_id);
	}
}


void Kernel::Execution_context::_unschedule()
{
	assert(__processor->id() == Processor::executing_id());
	__processor->scheduler()->remove(this);
}


void Kernel::Execution_context::_yield()
{
	assert(__processor->id() == Processor::executing_id());
	__processor->scheduler()->yield_occupation();
}
