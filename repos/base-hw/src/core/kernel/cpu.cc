/*
 * \brief   Class for kernel data that is needed to manage a specific CPU
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
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/irq.h>
#include <pic.h>
#include <timer.h>

/* base includes */
#include <unmanaged_singleton.h>

using namespace Kernel;

namespace Kernel
{
	/**
	 * Lists all pending domain updates
	 */
	class Cpu_domain_update_list;

	Timer * timer();

	Cpu_pool * cpu_pool() { return unmanaged_singleton<Cpu_pool>(); }
}

class Kernel::Cpu_domain_update_list
: public Double_list_typed<Cpu_domain_update>
{
	typedef Cpu_domain_update Update;

	public:

		/**
		 * Perform all pending domain updates on the executing CPU
		 */
		void do_each() { for_each([] (Update * const u) { u->_do(); }); }
};

namespace Kernel
{
	/**
	 * Return singleton of the CPU domain-udpate list
	 */
	Cpu_domain_update_list * cpu_domain_update_list() {
		return unmanaged_singleton<Cpu_domain_update_list>(); }
}


/*************
 ** Cpu_job **
 *************/

Cpu_job::~Cpu_job()
{
	if (!_cpu) { return; }
	_cpu->scheduler()->remove(this);
}

void Cpu_job::_activate_own_share() { _cpu->schedule(this); }


void Cpu_job::_deactivate_own_share()
{
	assert(_cpu->id() == Cpu::executing_id());
	_cpu->scheduler()->unready(this);
}


void Cpu_job::_yield()
{
	assert(_cpu->id() == Cpu::executing_id());
	_cpu->scheduler()->yield();
}


void Cpu_job::_interrupt(unsigned const cpu_id)
{
	/* determine handling for specific interrupt */
	unsigned irq_id;
	if (pic()->take_request(irq_id))

		/* is the interrupt a cpu-local one */
		if (!_cpu->interrupt(irq_id)) {

			/* it needs to be a user interrupt */
			User_irq * irq = User_irq::object(irq_id);
			if (irq) irq->occurred();
			else PWRN("Unknown interrupt %u", irq_id);
		}

	/* end interrupt request at controller */
	pic()->finish_request();
}


void Cpu_job::affinity(Cpu * const cpu)
{
	_cpu = cpu;
	_cpu->scheduler()->insert(this);
}


void Cpu_job::quota(unsigned const q)
{
	if (_cpu) { _cpu->scheduler()->quota(this, q); }
	else { Cpu_share::quota(q); }
}


/**************
 ** Cpu_idle **
 **************/

void Cpu_idle::proceed(unsigned const cpu) { mtc()->switch_to_user(this, cpu); }


/*********
 ** Cpu **
 *********/

void Cpu::schedule(Job * const job)
{
	if (_id == executing_id()) { _scheduler.ready(job); }
	else if (_scheduler.ready_check(job)) { trigger_ip_interrupt(); }
}


void Cpu::Ipi::occurred()
{
	cpu_domain_update_list()->do_each();
	pending = false;
}


void Cpu::Ipi::trigger(unsigned const cpu_id)
{
	if (pending) return;

	pic()->trigger_ip_interrupt(cpu_id);
	pending = true;
}


Cpu::Ipi::Ipi(Irq::Pool &p) : Irq(Pic::IPI, p) { }


/***********************
 ** Cpu_domain_update **
 ***********************/

void Cpu_domain_update::_do()
{
	/* perform domain update locally and get pending bit */
	unsigned const id = Cpu::executing_id();
	if (!_pending[id]) { return; }
	_domain_update();
	_pending[id] = false;

	/* check wether there are still CPUs pending */
	unsigned i = 0;
	for (; i < NR_OF_CPUS && !_pending[i]; i++) { }
	if (i < NR_OF_CPUS) { return; }

	/* as no CPU is pending anymore, end the domain update */
	cpu_domain_update_list()->remove(this);
	_cpu_domain_update_unblocks();
}


bool Cpu_domain_update::_do_global(unsigned const domain_id)
{
	/* perform locally and leave it at that if in uniprocessor mode */
	_domain_id = domain_id;
	_domain_update();
	if (NR_OF_CPUS == 1) { return false; }

	/* inform other CPUs and block until they are done */
	cpu_domain_update_list()->insert_tail(this);
	unsigned const cpu_id = Cpu::executing_id();
	for (unsigned i = 0; i < NR_OF_CPUS; i++) {
		if (i == cpu_id) { continue; }
		_pending[i] = true;
		cpu_pool()->cpu(i)->trigger_ip_interrupt();
	}
	return true;
}
