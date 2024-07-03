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

/* core includes */
#include <board.h>
#include <kernel/cpu_context.h>
#include <kernel/irq.h>
#include <kernel/inter_processor_work.h>
#include <kernel/thread.h>

namespace Kernel {

	/**
	 * Class for kernel data that is needed to manage a specific CPU
	 */
	class Cpu;

	/**
	 * Provides a CPU object for every available CPU
	 */
	class Cpu_pool;
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

class Kernel::Cpu : public Core::Cpu, private Irq::Pool, private Timeout
{
	private:

		using Job = Cpu_job;

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


			void init();


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
			Idle_thread(Board::Address_space_id_allocator &addr_space_id_alloc,
			            Irq::Pool                         &user_irq_pool,
			            Cpu_pool                          &cpu_pool,
			            Cpu                               &cpu,
			            Pd                                &core_pd);
		};

		struct Halt_job : Job
		{
			Halt_job() : Job (0, 0) { }

			void exception(Kernel::Cpu &) override { }

			void proceed(Kernel::Cpu &) override;

			Kernel::Cpu_job* helping_destination() override { return this; }
		} _halt_job { };

		enum State { RUN, HALT, SUSPEND };

		State          _state { RUN };
		unsigned const _id;
		Board::Pic     _pic;
		Timer          _timer;
		Scheduler      _scheduler;
		Idle_thread    _idle;
		Ipi            _ipi_irq;

		Inter_processor_work_list &_global_work_list;
		Inter_processor_work_list  _local_work_list {};

		void     _arch_init();
		unsigned _quota() const { return (unsigned)_timer.us_to_ticks(cpu_quota_us); }
		unsigned _fill() const  { return (unsigned)_timer.us_to_ticks(cpu_fill_us); }

	public:

		void next_state_halt()    { _state = HALT; };
		void next_state_suspend() { _state = SUSPEND; };

		State state() { return _state; }

		/**
		 * Construct object for CPU 'id'
		 */
		Cpu(unsigned                     const  id,
		    Board::Address_space_id_allocator  &addr_space_id_alloc,
		    Irq::Pool                          &user_irq_pool,
		    Cpu_pool                           &cpu_pool,
		    Pd                                 &core_pd,
		    Board::Global_interrupt_controller &global_irq_ctrl);

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
		bool handle_if_cpu_local_interrupt(unsigned const irq_id);

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
		Job & scheduled_job() {
			return *static_cast<Job *>(&_scheduler.current())->helping_destination(); }

		unsigned id() const { return _id; }
		Scheduler &scheduler() { return _scheduler; }

		Irq::Pool &irq_pool() { return *this; }

		Inter_processor_work_list & work_list() {
			return _local_work_list; }

		/**
		 * Return CPU's idle thread object
		 */
		Kernel::Thread &idle_thread() { return _idle; }

		void reinit_cpu()
		{
			_arch_init();
			_state = RUN;
		}
};


/*
 * See the comment above the 'Cpu' class definition.
 */
#pragma GCC diagnostic pop


class Kernel::Cpu_pool
{
	private:

		Inter_processor_work_list  _global_work_list {};
		unsigned                   _nr_of_cpus;
		Genode::Constructible<Cpu> _cpus[Board::NR_OF_CPUS];

	public:

		Cpu_pool(unsigned nr_of_cpus);

		void
		initialize_executing_cpu(Board::Address_space_id_allocator  &addr_space_id_alloc,
		                         Irq::Pool                          &user_irq_pool,
		                         Pd                                 &core_pd,
		                         Board::Global_interrupt_controller &global_irq_ctrl);

		/**
		 * Return whether CPU object is valid and is constructed.
		 */
		bool cpu_valid(unsigned const id) const {
			return id < _nr_of_cpus && _cpus[id].constructed(); }

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

		void for_each_cpu(auto const &fn)
		{
			for (unsigned i = 0; i < _nr_of_cpus; i++) fn(cpu(i));
		}

		Inter_processor_work_list & work_list() {
			return _global_work_list; }

		unsigned nr_of_cpus() { return _nr_of_cpus; }
};

#endif /* _CORE__KERNEL__CPU_H_ */
