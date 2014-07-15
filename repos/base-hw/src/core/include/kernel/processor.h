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
#include <kernel/scheduler.h>

namespace Kernel
{
	/**
	 * A single user of a multiplexable processor
	 */
	class Processor_client;

	/**
	 * Ability to do a domain update on all processors
	 */
	class Processor_domain_update;

	/**
	 * Multiplexes a single processor to multiple processor clients
	 */
	typedef Scheduler<Processor_client> Processor_scheduler;

	/**
	 * A multiplexable common instruction processor
	 */
	class Processor;
}

class Kernel::Processor_domain_update
:
	public Double_list_item<Processor_domain_update>
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
		void _perform_locally();

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
		bool _perform(unsigned const domain_id);

		/**
		 * Notice that the update isn't pending on any processor anymore
		 */
		virtual void _processor_domain_update_unblocks() = 0;
};

class Kernel::Processor_client : public Processor_scheduler::Item
{
	protected:

		Processor *    _processor;
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
		 * Constructor
		 *
		 * \param processor  kernel object of targeted processor
		 * \param priority   scheduling priority
		 */
		Processor_client(Processor * const processor, Priority const priority)
		:
			Processor_scheduler::Item(priority),
			_processor(processor)
		{ }

		/**
		 * Destructor
		 */
		~Processor_client()
		{
			if (!_scheduled()) { return; }
			_unschedule();
		}


		/***************
		 ** Accessors **
		 ***************/

		Cpu_lazy_state * lazy_state() { return &_lazy_state; }
};

class Kernel::Processor : public Cpu
{
	private:

		unsigned const      _id;
		Processor_scheduler _scheduler;
		bool                _ip_interrupt_pending;
		Timer * const       _timer;

		/**
		 * Reset the scheduling timer for a new scheduling interval
		 */
		void _reset_timer()
		{
			unsigned const tics = _timer->ms_to_tics(USER_LAP_TIME_MS);
			_timer->start_one_shot(tics, _id);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param id           kernel name of the processor object
		 * \param idle_client  client that gets scheduled on idle
		 * \param timer        timer that is used for scheduling the processor
		 */
		Processor(unsigned const id, Processor_client * const idle_client,
		          Timer * const timer)
		:
			_id(id), _scheduler(idle_client), _ip_interrupt_pending(false),
			_timer(timer)
		{ }

		/**
		 * Initializate on the processor that this object corresponds to
		 */
		void init_processor_local() { _reset_timer(); }

		/**
		 * Check for a scheduling timeout and handle it in case
		 *
		 * \param interrupt_id  kernel name of interrupt that caused this call
		 *
		 * \return  wether it was a timeout and therefore has been handled
		 */
		bool check_timer_interrupt(unsigned const interrupt_id)
		{
			if (_timer->interrupt_id(_id) != interrupt_id) { return false; }
			_scheduler.yield_occupation();
			_timer->clear_interrupt(_id);
			return true;
		}

		/**
		 * Notice that the inter-processor interrupt isn't pending anymore
		 */
		void ip_interrupt_handled() { _ip_interrupt_pending = false; }

		/**
		 * Raise the inter-processor interrupt of the processor
		 */
		void trigger_ip_interrupt();

		/**
		 * Add a processor client to the scheduling plan of the processor
		 *
		 * \param client  targeted client
		 */
		void schedule(Processor_client * const client);

		/**
		 * Handle exception of the processor and proceed its user execution
		 */
		void exception()
		{
			/*
			 * Request the current occupant without any update. While the
			 * processor was outside the kernel, another processor may have changed the
			 * scheduling of the local activities in a way that an update would return
			 * an occupant other than that whose exception caused the kernel entry.
			 */
			Processor_client * const old_client = _scheduler.occupant();
			Cpu_lazy_state * const old_state = old_client->lazy_state();
			old_client->exception(_id);

			/*
			 * The processor local as well as remote exception-handling may have
			 * changed the scheduling of the local activities. Hence we must update the
			 * occupant.
			 */
			bool update;
			Processor_client * const new_client = _scheduler.update_occupant(update);
			if (update) { _reset_timer(); }
			Cpu_lazy_state * const new_state = new_client->lazy_state();
			prepare_proceeding(old_state, new_state);
			new_client->proceed(_id);
		}

		/***************
		 ** Accessors **
		 ***************/

		unsigned id() const { return _id; }

		Processor_scheduler * scheduler() { return &_scheduler; }
};

#endif /* _KERNEL__PROCESSOR_H_ */
