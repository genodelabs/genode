/*
 * \brief   A multiplexable common instruction processor
 * \author  Martin Stein
 * \author  Stefan Kalkowski
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
#include <kernel/processor_pool.h>
#include <kernel/thread.h>
#include <kernel/irq.h>
#include <pic.h>
#include <timer.h>

namespace Kernel
{
	/**
	 * Lists all pending domain updates
	 */
	class Processor_domain_update_list;

	Pic * pic();
	Timer * timer();
}

class Kernel::Processor_domain_update_list
:
	public Double_list<Processor_domain_update>
{
	public:

		/**
		 * Perform all pending domain updates on the executing processor
		 */
		void for_each_perform_locally()
		{
			for_each([] (Processor_domain_update * const domain_update) {
				domain_update->_perform_locally();
			});
		}
};

namespace Kernel
{
	/**
	 * Return singleton of the processor domain-udpate list
	 */
	Processor_domain_update_list * processor_domain_update_list()
	{
		static Processor_domain_update_list s;
		return &s;
	}
}


/**********************
 ** Processor_client **
 **********************/

void Kernel::Processor_client::_interrupt(unsigned const processor_id)
{
	/* determine handling for specific interrupt */
	unsigned irq_id;
	Pic * const ic = pic();
	if (ic->take_request(irq_id)) {

		/* check wether the interrupt is a processor-scheduling timeout */
		if (!_processor->check_timer_interrupt(irq_id)) {

			/* check wether the interrupt is our inter-processor interrupt */
			if (ic->is_ip_interrupt(irq_id, processor_id)) {

				processor_domain_update_list()->for_each_perform_locally();
				_processor->ip_interrupt_handled();

			/* after all it must be a user interrupt */
			} else {

				/* try to inform the user interrupt-handler */
				Irq::occurred(irq_id);
			}
		}
	}
	/* end interrupt request at controller */
	ic->finish_request();
}


void Kernel::Processor_client::_schedule() { _processor->schedule(this); }


/***************
 ** Processor **
 ***************/

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
		if (_scheduler.insert_and_check(client)) { trigger_ip_interrupt(); }

	} else {

		/* add client locally */
		_scheduler.insert(client);
	}
}


void Kernel::Processor::trigger_ip_interrupt()
{
	if (!_ip_interrupt_pending) {
		pic()->trigger_ip_interrupt(_id);
		_ip_interrupt_pending = true;
	}
}


void Kernel::Processor_client::_unschedule()
{
	assert(_processor->id() == Processor::executing_id());
	_processor->scheduler()->remove(this);
}


void Kernel::Processor_client::_yield()
{
	assert(_processor->id() == Processor::executing_id());
	_processor->scheduler()->yield_occupation();
}


/*****************************
 ** Processor_domain_update **
 *****************************/

void Kernel::Processor_domain_update::_perform_locally()
{
	/* perform domain update locally and get pending bit */
	unsigned const processor_id = Processor::executing_id();
	if (!_pending[processor_id]) { return; }
	_domain_update();
	_pending[processor_id] = false;

	/* check wether there are still processors pending */
	unsigned i = 0;
	for (; i < PROCESSORS && !_pending[i]; i++) { }
	if (i < PROCESSORS) { return; }

	/* as no processors pending anymore, end the domain update */
	processor_domain_update_list()->remove(this);
	_processor_domain_update_unblocks();
}


bool Kernel::Processor_domain_update::_perform(unsigned const domain_id)
{
	/* perform locally and leave it at that if in uniprocessor mode */
	_domain_id = domain_id;
	_domain_update();
	if (PROCESSORS == 1) { return false; }

	/* inform other processors and block until they are done */
	processor_domain_update_list()->insert_tail(this);
	unsigned const processor_id = Processor::executing_id();
	for (unsigned i = 0; i < PROCESSORS; i++) {
		if (i == processor_id) { continue; }
		_pending[i] = true;
		processor_pool()->processor(i)->trigger_ip_interrupt();
	}
	return true;
}
