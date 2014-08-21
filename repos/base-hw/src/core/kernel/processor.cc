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
#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/irq.h>
#include <pic.h>
#include <timer.h>

using namespace Kernel;

namespace Kernel
{
	/**
	 * Lists all pending domain updates
	 */
	class Processor_domain_update_list;

	Pic * pic();
	Timer * timer();

	Processor_pool * processor_pool() {
		return unmanaged_singleton<Processor_pool>(); }
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
	Processor_domain_update_list * processor_domain_update_list() {
		return unmanaged_singleton<Processor_domain_update_list>(); }
}


/**********************
 ** Processor_client **
 **********************/

void Processor_client::_interrupt(unsigned const processor_id)
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


void Processor_client::_schedule() { _processor->schedule(this); }


/********************
 ** Processor_idle **
 ********************/

Cpu_idle::Cpu_idle(Processor * const cpu) : Processor_client(cpu, 0)
{
	cpu_exception = RESET;
	ip = (addr_t)&_main;
	sp = (addr_t)&_stack[stack_size];
	init_thread((addr_t)core_pd()->translation_table(), core_pd()->id());
}

void Cpu_idle::proceed(unsigned const cpu) { mtc()->continue_user(this, cpu); }


/***************
 ** Processor **
 ***************/

void Processor::schedule(Processor_client * const client)
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


void Processor::trigger_ip_interrupt()
{
	if (!_ip_interrupt_pending) {
		pic()->trigger_ip_interrupt(_id);
		_ip_interrupt_pending = true;
	}
}


void Processor_client::_unschedule()
{
	assert(_processor->id() == Processor::executing_id());
	_processor->scheduler()->remove(this);
}


void Processor_client::_yield()
{
	assert(_processor->id() == Processor::executing_id());
	_processor->scheduler()->yield_occupation();
}


/*****************************
 ** Processor_domain_update **
 *****************************/

void Processor_domain_update::_perform_locally()
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


bool Processor_domain_update::_perform(unsigned const domain_id)
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


void Kernel::Processor::exception()
{
	/*
	 * Request the current occupant without any update. While the
	 * processor was outside the kernel, another processor may have changed the
	 * scheduling of the local activities in a way that an update would return
	 * an occupant other than that whose exception caused the kernel entry.
	 */
	Processor_client * const old_client = _scheduler.occupant();
	Cpu_lazy_state * const old_state = old_client->lazy_state();
	old_client->exception(_id);

	/*
	 * The processor local as well as remote exception-handling may have
	 * changed the scheduling of the local activities. Hence we must update the
	 * occupant.
	 */
	bool updated, refreshed;
	Processor_client * const new_client =
		_scheduler.update_occupant(updated, refreshed);

	/**
	 * There are three scheduling situations we have to deal with:
	 *
	 * The client has not changed and didn't yield:
	 *
	 *    The client must not update its time-slice state as the timer
	 *    can continue as is and hence keeps providing the information.
	 *
	 * The client has changed or did yield and the previous client
	 * received a fresh timeslice:
	 *
	 *    The previous client can reset his time-slice state.
	 *    The timer must be re-programmed according to the time-slice
	 *    state of the new client.
	 *
	 * The client has changed and the previous client did not receive
	 * a fresh timeslice:
	 *
	 *    The previous client must update its time-slice state. The timer
	 *    must be re-programmed according to the time-slice state of the
	 *    new client.
	 */
	if (updated) {
		unsigned const tics_per_slice = _tics_per_slice();
		if (refreshed) { old_client->reset_tics_consumed(); }
		else {
			unsigned const tics_left = _timer->value(_id);
			old_client->update_tics_consumed(tics_left, tics_per_slice);
		}
		_update_timer(new_client->tics_consumed(), tics_per_slice);
	}
	/**
	 * Apply the CPU state of the new client and continue his execution
	 */
	Cpu_lazy_state * const new_state = new_client->lazy_state();
	prepare_proceeding(old_state, new_state);
	new_client->proceed(_id);
}
