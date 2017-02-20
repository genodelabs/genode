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

#ifndef _CORE__INCLUDE__KERNEL__CPU_H_
#define _CORE__INCLUDE__KERNEL__CPU_H_

/* core includes */
#include <kernel/clock.h>
#include <cpu.h>
#include <kernel/cpu_scheduler.h>
#include <kernel/irq.h>

namespace Genode { class Translation_table; }

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

class Kernel::Cpu_context : public Genode::Cpu::Context
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

class Kernel::Cpu_job : public Genode::Cpu::User_context, public Cpu_share
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

		/***************
		 ** Accessors **
		 ***************/

		void cpu(Cpu * const cpu) { _cpu = cpu; }
};

class Kernel::Cpu_idle : public Cpu_job
{
	private:

		static constexpr size_t stack_size = sizeof(addr_t) * 32;

		char _stack[stack_size] __attribute__((aligned(16)));

		/**
		 * Main function of all idle threads
		 */
		static void _main();

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

class Kernel::Cpu : public Genode::Cpu, public Irq::Pool, private Timeout
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
		Clock          _clock;
		Cpu_idle       _idle;
		Cpu_scheduler  _scheduler;
		Ipi            _ipi_irq;
		Irq            _timer_irq; /* timer irq implemented as empty event */

		unsigned _quota() const { return _clock.us_to_tics(cpu_quota_us); }
		unsigned _fill() const  { return _clock.us_to_tics(cpu_fill_us); }

	public:

		/**
		 * Construct object for CPU 'id' with scheduling timer 'timer'
		 */
		Cpu(unsigned const id, Timer * const timer);

		/**
		 * Initialize primary cpu object
		 *
		 * \param pic      interrupt controller object
		 * \param core_pd  core's pd object
		 * \param board    object encapsulating board specifics
		 */
		void init(Pic &pic/*, Kernel::Pd &core_pd, Genode::Board & board*/);

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
		bool interrupt(unsigned const irq_id);

		/**
		 * Schedule 'job' at this CPU
		 */
		void schedule(Job * const job);

		/**
		 * Return the job that should be executed at next
		 */
		Cpu_job& schedule();

		void set_timeout(Timeout * const timeout, time_t const duration_us);

		time_t timeout_age_us(Timeout const * const timeout) const;

		time_t timeout_max_us() const;

		/***************
		 ** Accessors **
		 ***************/

		/**
		 * Returns the currently active job
		 */
		Job & scheduled_job() const {
			return *static_cast<Job *>(_scheduler.head())->helping_sink(); }

		unsigned id() const { return _id; }
		Cpu_scheduler * scheduler() { return &_scheduler; }
};

class Kernel::Cpu_pool
{
	private:

		Timer _timer;

		/*
		 * Align to machine word size, otherwise load/stores might fail on some
		 * platforms.
		 */
		char _cpus[NR_OF_CPUS][sizeof(Cpu)]
		     __attribute__((aligned(sizeof(addr_t))));

	public:

		Cpu_pool();

		/**
		 * Return object of CPU 'id'
		 */
		Cpu * cpu(unsigned const id) const;

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

#endif /* _CORE__INCLUDE__KERNEL__CPU_H_ */
