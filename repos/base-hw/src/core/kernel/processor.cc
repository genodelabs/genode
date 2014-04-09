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

using namespace Kernel;

namespace Kernel
{
	Pic * pic();
	Timer * timer();
}

using Tlb_list_item = Genode::List_element<Processor_client>;
using Tlb_list      = Genode::List<Tlb_list_item>;


static Tlb_list *tlb_list()
{
	static Tlb_list tlb_list;
	return &tlb_list;
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

			_processor->scheduler()->yield_occupation();
			timer()->clear_interrupt(processor_id);

		/* check wether the interrupt is our inter-processor interrupt */
		} else if (ic->is_ip_interrupt(irq_id, processor_id)) {

			_processor->ip_interrupt();

		/* after all it must be a user interrupt */
		} else {

			/* try to inform the user interrupt-handler */
			Irq::occurred(irq_id);
		}
	}
	/* end interrupt request at controller */
	ic->finish_request();
}


void Kernel::Processor_client::_schedule() { _processor->schedule(this); }


void Kernel::Processor_client::tlb_to_flush(unsigned pd_id)
{
	/* initialize pd and reference counters, and remove client from scheduler */
	_flush_tlb_pd_id   = pd_id;
	for (unsigned i = 0; i < PROCESSORS; i++)
		_flush_tlb_ref_cnt[i] = false;
	_unschedule();

	/* find the last working item in the TLB work queue */
	Tlb_list_item * last = tlb_list()->first();
	while (last && last->next()) last = last->next();

	/* insert new work item at the end of the work list */
	tlb_list()->insert(&_flush_tlb_li, last);

	/* enforce kernel entry of other processors */
	for (unsigned i = 0; i < PROCESSORS; i++)
		pic()->trigger_ip_interrupt(i);

	processor_pool()->processor(Processor::executing_id())->flush_tlb();
}


void Kernel::Processor_client::flush_tlb_by_id()
{
	/* flush TLB on current processor and adjust ref counter */
	Processor::flush_tlb_by_pid(_flush_tlb_pd_id);
	_flush_tlb_ref_cnt[Processor::executing_id()] = true;

	/* check whether all processors are done */
	for (unsigned i = 0; i < PROCESSORS; i++)
		if (!_flush_tlb_ref_cnt[i]) return;

	/* remove work item from the list and re-schedule thread */
	tlb_list()->remove(&_flush_tlb_li);
	_schedule();
}


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
	assert(_processor->id() == Processor::executing_id());
	_processor->scheduler()->remove(this);
}


void Kernel::Processor_client::_yield()
{
	assert(_processor->id() == Processor::executing_id());
	_processor->scheduler()->yield_occupation();
}


void Kernel::Processor::flush_tlb()
{
	/* iterate through the list of TLB work items, and proceed them */
	for (Tlb_list_item * cli = tlb_list()->first(); cli;) {
		Tlb_list_item * current = cli;
		cli = current->next();
		current->object()->flush_tlb_by_id();
	}
}
