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

#ifndef _KERNEL__CPU_H_
#define _KERNEL__CPU_H_

/* core includes */
#include <translation_table.h>
#include <timer.h>
#include <cpu.h>
#include <kernel/cpu_scheduler.h>
#include <kernel/irq.h>

/* base includes */
#include <unmanaged_singleton.h>

namespace Kernel
{
	/**
	 * CPU context of a kernel stack
	 */
	class Cpu_context;

	/**
	 * Context of a job (thread, VM, idle) that shall be executed by a CPU
	 */
	class Cpu_job;

	/**
	 * Ability to do a domain update on all CPUs
	 */
	class Cpu_domain_update;

	/**
	 * Execution context that is scheduled on CPU idle
	 */
	class Cpu_idle;

	/**
	 * Class for kernel data that is needed to manage a specific CPU
	 */
	class Cpu;

	/**
	 * Provides a CPU object for every available CPU
	 */
	class Cpu_pool;

	/**
	 * Return singleton of CPU pool
	 */
	Cpu_pool * cpu_pool();
}

class Kernel::Cpu_context : Genode::Cpu::Context
{
	private:

		/**
		 * Hook for environment specific initializations
		 *
		 * \param stack_size  size of kernel stack
		 * \param table       base of transit translation table
		 */
		void _init(size_t const stack_size, addr_t const table);

	public:

		/**
		 * Constructor
		 *
		 * \param table  mode-transition table
		 */
		Cpu_context(Genode::Translation_table * const table);
};

class Kernel::Cpu_domain_update : public Double_list_item
{
	friend class Cpu_domain_update_list;

	private:

		bool     _pending[NR_OF_CPUS];
		unsigned _domain_id;

		/**
		 * Domain-update back-end
		 */
		void _domain_update() { Genode::Cpu::flush_tlb_by_pid(_domain_id); }

		/**
		 * Perform the domain update on the executing CPU
		 */
		void _do();

	protected:

		/**
		 * Constructor
		 */
		Cpu_domain_update()
		{
			for (unsigned i = 0; i < NR_OF_CPUS; i++) { _pending[i] = false; }
		}

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

		Cpu *          _cpu;
		Cpu_lazy_state _lazy_state;

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
		virtual void exception(unsigned const id) = 0;

		/**
		 * Continue execution on CPU 'id'
		 */
		virtual void proceed(unsigned const id) = 0;

		/**
		 * Return which job currently uses our CPU-share
		 */
		virtual Cpu_job * helping_sink() = 0;

		/**
		 * Construct a job with scheduling priority 'p' and time quota 'q'
		 */
		Cpu_job(Cpu_priority const p, unsigned const q)
		: Cpu_share(p, q), _cpu(0) { }

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

		/***************
		 ** Accessors **
		 ***************/

		void cpu(Cpu * const cpu) { _cpu = cpu; }
		Cpu_lazy_state * lazy_state() { return &_lazy_state; }
};

class Kernel::Cpu_idle : public Genode::Cpu::User_context, public Cpu_job
{
	private:

		static constexpr size_t stack_size = sizeof(addr_t) * 32;

		char _stack[stack_size] __attribute__((aligned(16)));

		/**
		 * Main function of all idle threads
		 */
		static void _main() { while (1) { Genode::Cpu::wait_for_interrupt(); } }

	public:

		/**
		 * Construct idle context for CPU 'cpu'
		 */
		Cpu_idle(Cpu * const cpu);


		/*
		 * Cpu_job interface
		 */

		void exception(unsigned const cpu);
		void proceed(unsigned const cpu_id);
		Cpu_job * helping_sink() { return this; }
};

class Kernel::Cpu : public Genode::Cpu,
                    public Irq::Pool
{
	private:

		typedef Cpu_job Job;

		/**
		 * Inter-processor-interrupt object of the cpu
		 */
		struct Ipi : Irq
		{
			bool pending = false;


			/*********************
			 **  Irq interface  **
			 *********************/

			void occurred();

			/**
			 * Constructor
			 *
			 * \param p  interrupt pool this irq shall reside in
			 */
			Ipi(Irq::Pool &p);

			/**
			 * Trigger the ipi
			 *
			 * \param cpu_id  id of the cpu this ipi object is related to
			 */
			void trigger(unsigned const cpu_id);
		};

		unsigned const _id;
		Cpu_idle       _idle;
		Timer * const  _timer;
		Cpu_scheduler  _scheduler;
		Ipi        _ipi_irq;
		Irq            _timer_irq; /* timer irq implemented as empty event */

		unsigned _quota() const { return _timer->ms_to_tics(cpu_quota_ms); }
		unsigned _fill() const  { return _timer->ms_to_tics(cpu_fill_ms); }

	public:

		/**
		 * Construct object for CPU 'id' with scheduling timer 'timer'
		 */
		Cpu(unsigned const id, Timer * const timer)
		: _id(id), _idle(this), _timer(timer),
		  _scheduler(&_idle, _quota(), _fill()),
		  _ipi_irq(*this),
		  _timer_irq(_timer->interrupt_id(_id), *this) { }

		/**
		 * Raise the IPI of the CPU
		 */
		void trigger_ip_interrupt() { _ipi_irq.trigger(_id); }

		/**
		 * Deliver interrupt to the CPU
		 *
		 * \param irq_id  id of the interrupt that occured
		 * \returns true if the interrupt belongs to this CPU, otherwise false
		 */
		bool interrupt(unsigned const irq_id)
		{
			Irq * const irq = object(irq_id);
			if (!irq) return false;
			irq->occurred();
			return true;
		}

		/**
		 * Schedule 'job' at this CPU
		 */
		void schedule(Job * const job);

		/**
		 * Handle recent exception of the CPU and proceed its user execution
		 */
		void exception()
		{
			/* update old job */
			Job * const old_job = scheduled_job();
			old_job->exception(_id);

			/* update scheduler */
			unsigned const old_time = _scheduler.head_quota();
			unsigned const new_time = _timer->value(_id);
			unsigned quota = old_time > new_time ? old_time - new_time : 1;
			_scheduler.update(quota);

			/* get new job */
			Job * const new_job = scheduled_job();
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

		/**
		 * Returns the currently active job
		 */
		Job * scheduled_job() const {
			return static_cast<Job *>(_scheduler.head())->helping_sink(); }

		unsigned id() const { return _id; }
		Cpu_scheduler * scheduler() { return &_scheduler; }
};

class Kernel::Cpu_pool
{
	private:

		Timer _timer;
		char  _cpus[NR_OF_CPUS][sizeof(Cpu)];

	public:

		/**
		 * Construct pool and thereby objects for all available CPUs
		 */
		Cpu_pool()
		{
			for (unsigned id = 0; id < NR_OF_CPUS; id++) {
				new (_cpus[id]) Cpu(id, &_timer); }
		}

		/**
		 * Return object of CPU 'id'
		 */
		Cpu * cpu(unsigned const id) const
		{
			assert(id < NR_OF_CPUS);
			char * const p = const_cast<char *>(_cpus[id]);
			return reinterpret_cast<Cpu *>(p);
		}

		/**
		 * Return object of primary CPU
		 */
		Cpu * primary_cpu() const { return cpu(Cpu::primary_id()); }

		/**
		 * Return object of current CPU
		 */
		Cpu * executing_cpu() const { return cpu(Cpu::executing_id()); }

		/*
		 * Accessors
		 */

		Timer * timer() { return &_timer; }
};

#endif /* _KERNEL__CPU_H_ */
