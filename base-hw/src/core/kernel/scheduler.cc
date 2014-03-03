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
	void reset_lap_time(unsigned const);
}


void Kernel::Execution_context::_interrupt(unsigned const processor_id)
{
	/* determine handling for specific interrupt */
	unsigned irq_id;
	if (pic()->take_request(irq_id))
	{
		/* check wether the interrupt is a scheduling timeout */
		if (timer()->interrupt_id(processor_id) == irq_id)
		{
			/* handle scheduling timeout */
			__processor->scheduler()->yield();
			timer()->clear_interrupt(processor_id);
			reset_lap_time(processor_id);
		} else {

			/* try to inform the user interrupt-handler */
			Irq::occurred(irq_id);
		}
	}
	/* end interrupt request at controller */
	pic()->finish_request();
}


void Kernel::Execution_context::_schedule()
{
	__processor->scheduler()->insert(this);
}


void Kernel::Execution_context::_unschedule()
{
	__processor->scheduler()->remove(this);
}


void Kernel::Execution_context::_yield()
{
	__processor->scheduler()->yield();
}
