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
: public Double_list_typed<Processor_domain_update>
{
	typedef Processor_domain_update Update;

	public:

		/**
		 * Perform all pending domain updates on the executing processor
		 */
		void do_each() { for_each([] (Update * const u) { u->_do(); }); }
};

namespace Kernel
{
	/**
	 * Return singleton of the processor domain-udpate list
	 */
	Processor_domain_update_list * processor_domain_update_list() {
		return unmanaged_singleton<Processor_domain_update_list>(); }
}


/*************
 ** Cpu_job **
 *************/

Cpu_job::~Cpu_job() { if (_cpu) { _cpu->scheduler()->remove(this); } }

void Cpu_job::_schedule() { _cpu->schedule(this); }


void Cpu_job::_unschedule()
{
	assert(_cpu->id() == Processor::executing_id());
	_cpu->scheduler()->unready(this);
}


void Cpu_job::_yield()
{
	assert(_cpu->id() == Processor::executing_id());
	_cpu->scheduler()->yield();
}


void Cpu_job::_interrupt(unsigned const processor_id)
{
	/* determine handling for specific interrupt */
	unsigned irq_id;
	Pic * const ic = pic();
	if (ic->take_request(irq_id)) {

		/* check wether the interrupt is a processor-scheduling timeout */
		if (!_cpu->timer_irq(irq_id)) {

			/* check wether the interrupt is our inter-processor interrupt */
			if (ic->is_ip_interrupt(irq_id, processor_id)) {

				processor_domain_update_list()->do_each();
				_cpu->ip_interrupt_handled();

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


void Cpu_job::affinity(Processor * const cpu)
{
	_cpu = cpu;
	_cpu->scheduler()->insert(this);
}


/**************
 ** Cpu_idle **
 **************/

Cpu_idle::Cpu_idle(Processor * const cpu) : Cpu_job(Cpu_priority::min)
{
	Cpu_job::cpu(cpu);
	cpu_exception = RESET;
	ip = (addr_t)&_main;
	sp = (addr_t)&_stack[stack_size];
	init_thread((addr_t)core_pd()->translation_table(), core_pd()->id());
}

void Cpu_idle::proceed(unsigned const cpu) { mtc()->continue_user(this, cpu); }


/***************
 ** Processor **
 ***************/

void Processor::schedule(Job * const job)
{
	if (_id == executing_id()) { _scheduler.ready(job); }
	else if (_scheduler.ready_check(job)) { trigger_ip_interrupt(); }
}


void Processor::trigger_ip_interrupt()
{
	if (!_ip_interrupt_pending) {
		pic()->trigger_ip_interrupt(_id);
		_ip_interrupt_pending = true;
	}
}


/*****************************
 ** Processor_domain_update **
 *****************************/

void Processor_domain_update::_do()
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


bool Processor_domain_update::_do_global(unsigned const domain_id)
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
