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
#include <kernel/scheduler.h>
#include <kernel/timer.h>

namespace Kernel {

	class Cpu;

	/**
	 * Context of a job (thread, VM, idle) that shall be executed by a CPU
	 */
	class Cpu_job;
}


class Kernel::Cpu_job : private Scheduler::Context
{
	private:

		friend class Cpu; /* static_cast from 'Scheduler::Context' to 'Cpu_job' */

		time_t _execution_time { 0 };

		/*
		 * Noncopyable
		 */
		Cpu_job(Cpu_job const &);
		Cpu_job &operator = (Cpu_job const &);

	protected:

		Cpu * _cpu;

		/**
		 * Handle interrupt exception that occured during execution on CPU 'id'
		 */
		void _interrupt(Irq::Pool &user_irq_pool, unsigned const id);

		/**
		 * Activate our own CPU-share
		 */
		void _activate();

		/**
		 * Deactivate our own CPU-share
		 */
		void _deactivate();

		/**
		 * Yield the currently scheduled CPU share of this context
		 */
		void _yield();

		/**
		 * Return wether we are allowed to help job 'j' with our CPU-share
		 */
		bool _helping_possible(Cpu_job const &j) const { return j._cpu == _cpu; }

		using Context::ready;
		using Context::helping_finished;

		void help(Cpu_job &job) { Context::help(job); }

	public:

		using Context  = Scheduler::Context;
		using Priority = Scheduler::Priority;

		/**
		 * Handle exception that occured during execution on CPU 'id'
		 */
		virtual void exception(Cpu & cpu) = 0;

		/**
		 * Continue execution on CPU 'id'
		 */
		virtual void proceed(Cpu & cpu) = 0;

		/**
		 * Construct a job with scheduling priority 'p' and time quota 'q'
		 */
		Cpu_job(Priority const p, unsigned const q);

		/**
		 * Destructor
		 */
		virtual ~Cpu_job();

		/**
		 * Link job to CPU 'cpu'
		 */
		void affinity(Cpu &cpu);

		/**
		 * Set CPU quota of the job to 'q'
		 */
		void quota(unsigned const q);

		/**
		 * Update total execution time
		 */
		void update_execution_time(time_t duration) { _execution_time += duration; }

		/**
		 * Return total execution time
		 */
		time_t execution_time() const { return _execution_time; }


		/***************
		 ** Accessors **
		 ***************/

		void cpu(Cpu &cpu) { _cpu = &cpu; }
};

#endif /* _CORE__KERNEL__CPU_CONTEXT_H_ */
