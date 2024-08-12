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
	class Cpu_context;
}


/**
 * Context (thread, vcpu) that shall be executed by a CPU
 */
class Kernel::Cpu_context : private Scheduler::Context
{
	private:

		friend class Cpu;

		time_t _execution_time { 0 };
		Cpu   *_cpu_ptr;

		/*
		 * Noncopyable
		 */
		Cpu_context(Cpu_context const &);
		Cpu_context &operator = (Cpu_context const &);

	protected:

		Cpu &_cpu() const { return *_cpu_ptr; }

		/**
		 * Handle interrupt exception
		 */
		void _interrupt(Irq::Pool &user_irq_pool);

		void _activate();
		void _deactivate();

		/**
		 * Yield the currently scheduled CPU share of this context
		 */
		void _yield();

		/**
		 * Return possibility to help context 'j' scheduling-wise
		 */
		bool _helping_possible(Cpu_context const &j) const {
			return j._cpu_ptr == _cpu_ptr; }

		void _help(Cpu_context &context) { Context::help(context); }

		using Context::ready;
		using Context::helping_finished;

	public:

		using Context  = Scheduler::Context;
		using Priority = Scheduler::Priority;

		Cpu_context(Cpu           &cpu,
		            Priority const priority,
		            unsigned const quota);

		virtual ~Cpu_context();

		/**
		 * Link context to CPU 'cpu'
		 */
		void affinity(Cpu &cpu);

		/**
		 * Set CPU quota of the context to 'q'
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

		/**
		 * Handle exception that occured during execution of this context
		 */
		virtual void exception() = 0;

		/**
		 * Continue execution of this context
		 */
		virtual void proceed() = 0;
};

#endif /* _CORE__KERNEL__CPU_CONTEXT_H_ */
