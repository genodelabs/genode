/*
 * \brief   Class for kernel data that is needed to manage a specific CPU
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2014-01-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__CPU_CONTEXT_H_
#define _CORE__KERNEL__CPU_CONTEXT_H_

/* core includes */
#include <kernel/cpu_scheduler.h>
#include <kernel/timer.h>

namespace Kernel
{
	class Cpu;

	/**
	 * Context of a job (thread, VM, idle) that shall be executed by a CPU
	 */
	class Cpu_job;

	/**
	 * Ability to do a domain update on all CPUs
	 */
	class Cpu_domain_update;
}

class Kernel::Cpu_domain_update : public Double_list_item
{
	friend class Cpu_domain_update_list;

	private:

		bool     _pending[NR_OF_CPUS];
		unsigned _domain_id;

		/**
		 * Domain-update back-end
		 */
		void _domain_update();

		/**
		 * Perform the domain update on the executing CPU
		 */
		void _do();

	protected:

		Cpu_domain_update();

		/**
		 * Do an update of domain 'id' on all CPUs and return if this blocks
		 */
		bool _do_global(unsigned const id);

		/**
		 * Notice that the update isn't pending on any CPU anymore
		 */
		virtual void _cpu_domain_update_unblocks() = 0;
};

class Kernel::Cpu_job : public Cpu_share
{
	protected:

		Cpu * _cpu;

		/**
		 * Handle interrupt exception that occured during execution on CPU 'id'
		 */
		void _interrupt(unsigned const id);

		/**
		 * Activate our own CPU-share
		 */
		void _activate_own_share();

		/**
		 * Deactivate our own CPU-share
		 */
		void _deactivate_own_share();

		/**
		 * Yield the currently scheduled CPU share of this context
		 */
		void _yield();

		/**
		 * Return wether we are allowed to help job 'j' with our CPU-share
		 */
		bool _helping_possible(Cpu_job * const j) { return j->_cpu == _cpu; }

	public:

		/**
		 * Handle exception that occured during execution on CPU 'id'
		 */
		virtual void exception(Cpu & cpu) = 0;

		/**
		 * Continue execution on CPU 'id'
		 */
		virtual void proceed(Cpu & cpu) = 0;

		/**
		 * Return which job currently uses our CPU-share
		 */
		virtual Cpu_job * helping_sink() = 0;

		/**
		 * Construct a job with scheduling priority 'p' and time quota 'q'
		 */
		Cpu_job(Cpu_priority const p, unsigned const q);

		/**
		 * Destructor
		 */
		~Cpu_job();

		/**
		 * Link job to CPU 'cpu'
		 */
		void affinity(Cpu * const cpu);

		/**
		 * Set CPU quota of the job to 'q'
		 */
		void quota(unsigned const q);

		/**
		 * Return wether our CPU-share is currently active
		 */
		bool own_share_active() { return Cpu_share::ready(); }

		void timeout(Timeout * const timeout, time_t const duration_us);

		time_t timeout_age_us(Timeout const * const timeout) const;

		time_t timeout_max_us() const;

		time_t time() const;

		/***************
		 ** Accessors **
		 ***************/

		void cpu(Cpu * const cpu) { _cpu = cpu; }
};

#endif /* _CORE__KERNEL__CPU_CONTEXT_H_ */
