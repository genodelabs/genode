/*
 * \brief  ARM with SMP support specific aspects of the kernel cpu objects
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2016-01-07
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/cpu.h>

namespace Kernel
{
	/**
	 * Lists all pending domain updates
	 */
	class Cpu_domain_update_list;
}


class Kernel::Cpu_domain_update_list
: public Double_list_typed<Cpu_domain_update>
{
	public:

		/**
		 * Perform all pending domain updates on the executing CPU
		 */
		void do_each() {
			for_each([] (Cpu_domain_update * const u) { u->_do(); }); }
};


using namespace Kernel;

/**
 * Return singleton of the CPU domain-udpate list
 */
Cpu_domain_update_list & cpu_domain_update_list() {
	return *unmanaged_singleton<Cpu_domain_update_list>(); }



void Cpu::Ipi::occurred()
{
	cpu_domain_update_list().do_each();
	pending = false;
}


void Cpu::Ipi::trigger(unsigned const cpu_id)
{
	if (pending) return;

	pic()->send_ipi(cpu_id);
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
	if (!_pending[id]) return;

	_domain_update();
	_pending[id] = false;

	/* check wether there are still CPUs pending */
	for (unsigned i = 0; i < NR_OF_CPUS; i++)
		if (_pending[i]) return;

	/* as no CPU is pending anymore, end the domain update */
	cpu_domain_update_list().remove(this);
	_cpu_domain_update_unblocks();
}


bool Cpu_domain_update::_do_global(unsigned const domain_id)
{
	/* perform locally and leave it at that if in uniprocessor mode */
	_domain_id = domain_id;
	_domain_update();
	if (NR_OF_CPUS == 1) return false;

	/* inform other CPUs and block until they are done */
	cpu_domain_update_list().insert_tail(this);
	unsigned const cpu_id = Cpu::executing_id();
	for (unsigned i = 0; i < NR_OF_CPUS; i++) {
		if (i == cpu_id) continue;
		_pending[i] = true;
		cpu_pool()->cpu(i)->trigger_ip_interrupt();
	}
	return true;
}
