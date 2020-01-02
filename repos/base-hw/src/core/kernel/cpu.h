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

#ifndef _CORE__KERNEL__CPU_H_
#define _CORE__KERNEL__CPU_H_

#include <util/reconstructible.h>

/* core includes */
#include <board.h>
#include <kernel/cpu_context.h>
#include <kernel/irq.h>
#include <kernel/inter_processor_work.h>
#include <kernel/thread.h>

namespace Kernel
{
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
	Cpu_pool &cpu_pool();
}


/*
 * The 'Cpu' class violates the "Effective C++" practices because it publicly
 * inherits the 'Genode::Cpu' base class, which does not have a virtual
 * destructor. Since 'Cpu' implements the 'Timeout' interface, however, it has
 * a vtable.
 *
 * Adding a virtual destructor in the base class would be unnatural as the base
 * class hierarchy does not represent an abstract interface.
 *
 * Inheriting the 'Genode::Cpu' class privately is not an option because the
 * user of 'Cpu' class expects architecture-specific register definitions to be
 * provided by 'Cpu'. Hence, all those architecture- specific definitions would
 * end up as 'using' clauses in the generic class.
 *
 * XXX Remove the disabled warning, e.g., by one of the following approaches:
 *
 * * Prevent 'Cpu' to have virtual methods by making 'Timeout' a member instead
 *   of a base class.
 *
 * * Change the class hierarchy behind 'Genode::Cpu' such that
 *   architecture-specific bits do no longer need to implicitly become part
 *   of the public interface of 'Cpu'. For example, register-definition types
 *   could all be embedded in an 'Arch_regs' type, which the 'Cpu' class could
 *   publicly provide via a 'typedef Genode::Cpu::Arch_regs Arch_regs'.
 *   Then, the 'Genode::Cpu' could be inherited privately.
 */
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"

class Kernel::Cpu : public Genode::Cpu, private Irq::Pool, private Timeout
{
	private:

		typedef Cpu_job Job;

		/**
		 * Inter-processor-interrupt object of the cpu
		 */
		struct Ipi : Irq
		{
			Cpu & cpu;
			bool  pending { false };

			/**
			 * Constructor
			 *
			 * \param cpu  cpu this IPI belongs to
			 */
			Ipi(Cpu & cpu);


			/*********************
			 **  Irq interface  **
			 *********************/

			void occurred() override;
		};

		friend void Ipi::occurred(void);

		struct Idle_thread : Kernel::Thread
		{
			/**
			 * Construct idle context for CPU 'cpu'
			 */
			Idle_thread(Cpu &cpu);
		};


		unsigned const _id;
		Board::Pic     _pic {};
		Timer          _timer;
		Cpu_scheduler  _scheduler;
		Idle_thread    _idle;
		Ipi            _ipi_irq;

		Inter_processor_work_list &_global_work_list;
		Inter_processor_work_list  _local_work_list {};

		void     _arch_init();
		unsigned _quota() const { return _timer.us_to_ticks(cpu_quota_us); }
		unsigned _fill() const  { return _timer.us_to_ticks(cpu_fill_us); }

	public:

		enum { KERNEL_STACK_SIZE = 16 * 1024 * sizeof(Genode::addr_t) };

		/**
		 * Construct object for CPU 'id'
		 */
		Cpu(unsigned const id,
		    Inter_processor_work_list & global_work_list);

		static inline unsigned primary_id() { return 0; }

		/**
		 * Raise the IPI of the CPU
		 */
		void trigger_ip_interrupt();

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

		Board::Pic & pic()   { return _pic; }
		Timer      & timer() { return _timer; }

		addr_t stack_start();

		/**
		 * Returns the currently active job
		 */
		Job & scheduled_job() const {
			return *static_cast<Job *>(&_scheduler.head())->helping_sink(); }

		unsigned id() const { return _id; }
		Cpu_scheduler &scheduler() { return _scheduler; }

		Irq::Pool &irq_pool() { return *this; }

		Inter_processor_work_list & work_list() {
			return _local_work_list; }

		/**
		 * Return CPU's idle thread object
		 */
		Kernel::Thread &idle_thread() { return _idle; }
};


/*
 * See the comment above the 'Cpu' class definition.
 */
#pragma GCC diagnostic pop


class Kernel::Cpu_pool
{
	private:

		Inter_processor_work_list  _global_work_list {};
		unsigned                   _count;
		unsigned                   _initialized { _count };
		Genode::Constructible<Cpu> _cpus[NR_OF_CPUS];

	public:

		Cpu_pool();

		bool initialize();

		/**
		 * Return object of CPU 'id'
		 */
		Cpu & cpu(unsigned const id);

		/**
		 * Return object of primary CPU
		 */
		Cpu & primary_cpu() { return cpu(Cpu::primary_id()); }

		/**
		 * Return object of current CPU
		 */
		Cpu & executing_cpu() { return cpu(Cpu::executing_id()); }

		template <typename FUNC>
		void for_each_cpu(FUNC const &func)
		{
			for (unsigned i = 0; i < _count; i++) func(cpu(i));
		}

		Inter_processor_work_list & work_list() {
			return _global_work_list; }
};

#endif /* _CORE__KERNEL__CPU_H_ */
