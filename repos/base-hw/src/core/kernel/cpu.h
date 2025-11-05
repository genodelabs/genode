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


class Kernel::Cpu : public Board::Cpu, private Irq::Pool,
                    public Genode::List<Cpu>::Element
{
	public:

		using Context = Cpu_context;

	private:

		friend class Cpu_context;

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

		struct Halt_job : Cpu_context
		{
			Halt_job(Cpu &cpu)
			: Cpu_context(cpu, Scheduler::Group_id::BACKGROUND) { }

			void exception(Genode::Cpu_state&) override { }
			void proceed()   override;
		} _halt_job { *this };

		enum State { RUN, HALT, SUSPEND };

		Cpu_pool &_pool;

		State       _state { RUN };
		Id    const _id;
		Board::Local_interrupt_controller _pic;
		Timer       _timer;
		Idle_thread _idle;
		Scheduler   _scheduler;
		Ipi         _ipi_irq;

		Inter_processor_work_list _local_work_list {};

		void _arch_init();

	public:

		void next_state_halt()    { _state = HALT; };
		void next_state_suspend() { _state = SUSPEND; };

		State state() { return _state; }

		/**
		 * Construct object for CPU 'id'
		 */
		Cpu(Id const id, Cpu_pool &cpu_pool, Pd &core_pd);

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
		 * Assign 'context' to this CPU
		 */
		void assign(Context& context);

		/**
		 * Return the context that should be executed next
		 */
		Context& schedule_next_context();

		void backtrace();

		Board::Local_interrupt_controller & pic() { return _pic; }

		Timer & timer() { return _timer; }

		addr_t stack_base();
		addr_t stack_start();

		/**
		 * Returns the currently scheduled context
		 */
		Context & current_context() {
			return static_cast<Context&>(_scheduler.current().helping_destination()); }

		Id id() const { return _id; }
		Scheduler &scheduler() { return _scheduler; }

		Irq::Pool &irq_pool() { return *this; }

		Inter_processor_work_list &work_list() {
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

		[[noreturn]] void panic(Genode::Cpu_state &state);
};


class Kernel::Cpu_pool
{
	private:

		Inter_processor_work_list _global_work_list {};

		Board::Global_interrupt_controller _global_irq_ctrl { };

		Irq::Pool _user_irq_pool {};

		Genode::List<Cpu> _cpus {};

		friend class Cpu;

	public:

		void initialize_executing_cpu(Pd &core_pd);

		Cpu & cpu(Cpu::Id const id);

		void with_cpu(Call_arg arg, auto const &fn)
		{
			int id = (int) arg;
			Cpu * cpu = _cpus.first();
			for (int idx = 0; cpu; idx++) {
				if (id == idx) fn(*cpu);
				cpu = cpu->next();
			}
		}

		/**
		 * Return object of primary CPU
		 */
		Cpu & primary_cpu() { return *_cpus.first(); }

		void for_each_cpu(auto const &fn)
		{
			Cpu * c = _cpus.first();
			while (c) {
				fn(*c);
				c = c->next();
			}
		}

		Inter_processor_work_list &work_list() {
			return _global_work_list; }

		Irq::Pool & irq_pool() { return _user_irq_pool; }

		void resume() { _global_irq_ctrl.resume(); }
};

#endif /* _CORE__KERNEL__CPU_H_ */
