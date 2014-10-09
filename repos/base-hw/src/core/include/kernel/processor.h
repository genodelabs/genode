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

#ifndef _KERNEL__PROCESSOR_H_
#define _KERNEL__PROCESSOR_H_

/* core includes */
#include <timer.h>
#include <cpu.h>
#include <kernel/cpu_scheduler.h>

/* base includes */
#include <unmanaged_singleton.h>

namespace Kernel
{
	/**
	 * Context of a job (thread, VM, idle) that shall be executed by a CPU
	 */
	class Cpu_job;

	/**
	 * Ability to do a domain update on all processors
	 */
	class Processor_domain_update;

	/**
	 * Execution context that is scheduled on CPU idle
	 */
	class Cpu_idle;

	/**
	 * A multiplexable common instruction processor
	 */
	class Processor;

	/**
	 * Provides a processor object for every available processor
	 */
	class Processor_pool;

	/**
	 * Return Processor_pool singleton
	 */
	Processor_pool * processor_pool();
}

class Kernel::Processor_domain_update : public Double_list_item
{
	friend class Processor_domain_update_list;

	private:

		bool     _pending[PROCESSORS];
		unsigned _domain_id;

		/**
		 * Domain-update back-end
		 */
		void _domain_update() { Cpu::flush_tlb_by_pid(_domain_id); }

		/**
		 * Perform the domain update on the executing processors
		 */
		void _do();

	protected:

		/**
		 * Constructor
		 */
		Processor_domain_update()
		{
			for (unsigned i = 0; i < PROCESSORS; i++) { _pending[i] = false; }
		}

		/**
		 * Perform the domain update on all processors
		 *
		 * \param domain_id  kernel name of targeted domain
		 *
		 * \return  wether the update blocks and reports back on completion
		 */
		bool _do_global(unsigned const domain_id);

		/**
		 * Notice that the update isn't pending on any processor anymore
		 */
		virtual void _processor_domain_update_unblocks() = 0;
};

class Kernel::Cpu_job : public Cpu_share
{
	protected:

		Processor *    _cpu;
		Cpu_lazy_state _lazy_state;

		/**
		 * Handle an interrupt exception that occured during execution
		 *
		 * \param processor_id  kernel name of targeted processor
		 */
		void _interrupt(unsigned const processor_id);

		/**
		 * Insert context into the processor scheduling
		 */
		void _schedule();

		/**
		 * Remove context from the processor scheduling
		 */
		void _unschedule();

		/**
		 * Yield currently scheduled processor share of the context
		 */
		void _yield();

	public:

		/**
		 * Handle an exception that occured during execution
		 *
		 * \param processor_id  kernel name of targeted processor
		 */
		virtual void exception(unsigned const processor_id) = 0;

		/**
		 * Continue execution
		 *
		 * \param processor_id  kernel name of targeted processor
		 */
		virtual void proceed(unsigned const processor_id) = 0;

		/**
		 * Construct a job with scheduling priority 'prio'
		 */
		Cpu_job(Cpu_priority const p) : Cpu_share(p, 0), _cpu(0) { }

		/**
		 * Destructor
		 */
		~Cpu_job();

		/**
		 * Link job to CPU 'cpu'
		 */
		void affinity(Processor * const cpu);

		/***************
		 ** Accessors **
		 ***************/

		void cpu(Processor * const cpu) { _cpu = cpu; }
		Cpu_lazy_state * lazy_state() { return &_lazy_state; }
};

class Kernel::Cpu_idle : public Cpu::User_context, public Cpu_job
{
	private:

		static constexpr size_t stack_size = sizeof(addr_t) * 32;

		char _stack[stack_size] __attribute__((aligned(16)));

		/**
		 * Main function of all idle threads
		 */
		static void _main() { while (1) { Cpu::wait_for_interrupt(); } }

	public:

		/**
		 * Construct idle context for CPU 'cpu'
		 */
		Cpu_idle(Processor * const cpu);

		/**
		 * Handle exception that occured during execution on CPU 'cpu'
		 */
		void exception(unsigned const cpu)
		{
			switch (cpu_exception) {
			case INTERRUPT_REQUEST:      _interrupt(cpu); return;
			case FAST_INTERRUPT_REQUEST: _interrupt(cpu); return;
			case RESET:                                   return;
			default: assert(0); }
		}

		/**
		 * Continue execution on CPU 'cpu_id'
		 */
		void proceed(unsigned const cpu_id);
};

class Kernel::Processor : public Cpu
{
	private:

		typedef Cpu_job Job;

		unsigned const _id;
		Cpu_idle       _idle;
		Timer * const  _timer;
		Cpu_scheduler  _scheduler;
		bool           _ip_interrupt_pending;

		unsigned _quota() const { return _timer->ms_to_tics(cpu_quota_ms); }
		unsigned _fill() const { return _timer->ms_to_tics(cpu_fill_ms); }
		Job * _head() const { return static_cast<Job *>(_scheduler.head()); }

	public:

		/**
		 * Construct object for CPU 'id' with scheduling timer 'timer'
		 */
		Processor(unsigned const id, Timer * const timer)
		:
			_id(id), _idle(this), _timer(timer),
			_scheduler(&_idle, _quota(), _fill()),
			_ip_interrupt_pending(false) { }

		/**
		 * Check if IRQ 'i' was due to a scheduling timeout
		 */
		bool timer_irq(unsigned const i) { return _timer->interrupt_id(_id) == i; }

		/**
		 * Notice that the inter-processor interrupt isn't pending anymore
		 */
		void ip_interrupt_handled() { _ip_interrupt_pending = false; }

		/**
		 * Raise the inter-processor interrupt of the processor
		 */
		void trigger_ip_interrupt();

		/**
		 * Schedule 'job' at this CPU
		 */
		void schedule(Job * const job);

		/**
		 * Handle exception of the processor and proceed its user execution
		 */
		void exception()
		{
			/* update old job */
			Job * const old_job = _head();
			old_job->exception(_id);

			/* update scheduler */
			unsigned const old_time = _scheduler.head_quota();
			unsigned const new_time = _timer->value(_id);
			unsigned quota = old_time > new_time ? old_time - new_time : 1;
			_scheduler.update(quota);

			/* get new job */
			Job * const new_job = _head();
			quota = _scheduler.head_quota();
			assert(quota);
			_timer->start_one_shot(quota, _id);

			/* switch between lazy state of old and new job */
			Cpu_lazy_state * const old_state = old_job->lazy_state();
			Cpu_lazy_state * const new_state = new_job->lazy_state();
			prepare_proceeding(old_state, new_state);

			/* resume new job */
			new_job->proceed(_id);
		}

		/***************
		 ** Accessors **
		 ***************/

		unsigned id() const { return _id; }
		Cpu_scheduler * scheduler() { return &_scheduler; }
};

class Kernel::Processor_pool
{
	private:

		Timer _timer;
		char  _processors[PROCESSORS][sizeof(Processor)];

	public:

		/**
		 * Constructor
		 */
		Processor_pool()
		{
			for (unsigned id = 0; id < PROCESSORS; id++) {
				new (_processors[id]) Processor(id, &_timer); }
		}

		/**
		 * Return the kernel object of processor 'id'
		 */
		Processor * processor(unsigned const id) const
		{
			assert(id < PROCESSORS);
			char * const p = const_cast<char *>(_processors[id]);
			return reinterpret_cast<Processor *>(p);
		}

		/**
		 * Return the object of the primary processor
		 */
		Processor * primary_processor() const {
			return processor(Processor::primary_id()); }
};

#endif /* _KERNEL__PROCESSOR_H_ */
