/*
 * \brief   Kernel representation of a user thread
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__KERNEL__THREAD_H_
#define _CORE__KERNEL__THREAD_H_

/* core includes */
#include <kernel/signal_receiver.h>
#include <kernel/ipc_node.h>
#include <kernel/configuration.h>
#include <kernel/scheduler.h>
#include <kernel/object.h>
#include <kernel/irq_receiver.h>
#include <cpu.h>
#include <tlb.h>
#include <timer.h>

namespace Genode
{
	class Platform_thread;
}

namespace Kernel
{
	typedef Genode::Cpu             Cpu;
	typedef Genode::Page_flags      Page_flags;
	typedef Genode::Core_tlb        Core_tlb;
	typedef Genode::Pagefault       Pagefault;
	typedef Genode::Native_utcb     Native_utcb;

	class Schedule_context;
	typedef Scheduler<Schedule_context> Cpu_scheduler;

	/**
	 * Kernel object that can be scheduled for the CPU
	 */
	class Schedule_context : public Cpu_scheduler::Item
	{
		public:

			virtual void handle_exception() = 0;
			virtual void proceed() = 0;
	};

	/**
	 * Kernel representation of a user thread
	 */
	class Thread : public Cpu::User_context,
	               public Object<Thread, MAX_THREADS>,
	               public Schedule_context,
	               public Ipc_node,
	               public Irq_receiver
	{
		enum State
		{
			SCHEDULED,
			AWAIT_START,
			AWAIT_IPC,
			AWAIT_RESUMPTION,
			AWAIT_IRQ,
			AWAIT_SIGNAL,
			AWAIT_SIGNAL_CONTEXT_DESTRUCT,
			CRASHED,
		};

		Platform_thread * const _platform_thread; /* userland object wich
		                                           * addresses this thread */
		State _state; /* thread state, description given at the beginning */
		Pagefault _pagefault; /* last pagefault triggered by this thread */
		Thread * _pager; /* gets informed if thread throws a pagefault */
		unsigned _pd_id; /* ID of the PD this thread runs on */
		Native_utcb * _phys_utcb; /* physical UTCB base */
		Native_utcb * _virt_utcb; /* virtual UTCB base */
		Signal_receiver * _signal_receiver;
		Signal_handler    _signal_handler;

		/**
		 * Resume execution
		 */
		void _schedule();


		/**************
		 ** Ipc_node **
		 **************/

		void _has_received(size_t const s);

		void _awaits_receipt();


		/***************
		 ** Irq_owner **
		 ***************/

		void _received_irq();

		void _awaits_irq();

		public:

			void receive_signal(void * const base, size_t const size)
			{
				assert(_state == AWAIT_SIGNAL);
				assert(size <= phys_utcb()->size())
				Genode::memcpy(phys_utcb()->base(), base, size);
				_schedule();
			}

			void * operator new (size_t, void * p) { return p; }

			/**
			 * Constructor
			 */
			Thread(Platform_thread * const platform_thread)
			:
				_platform_thread(platform_thread), _state(AWAIT_START),
				_pager(0), _pd_id(0), _phys_utcb(0), _virt_utcb(0),
				_signal_receiver(0), _signal_handler((unsigned)this)
			{ }

			/**
			 * Suspend the thread due to unrecoverable misbehavior
			 */
			void crash();

			/**
			 * Prepare thread to get scheduled the first time
			 *
			 * \param ip         initial instruction pointer
			 * \param sp         initial stack pointer
			 * \param cpu_id     target cpu
			 * \param pd_id      target protection-domain
			 * \param utcb_phys  physical UTCB pointer
			 * \param utcb_virt  virtual UTCB pointer
			 */
			void prepare_to_start(void * const        ip,
			                      void * const        sp,
			                      unsigned const      cpu_id,
			                      unsigned const      pd_id,
			                      Native_utcb * const utcb_phys,
			                      Native_utcb * const utcb_virt);

			/**
			 * Start this thread
			 *
			 * \param ip         initial instruction pointer
			 * \param sp         initial stack pointer
			 * \param cpu_id     target cpu
			 * \param pd_id      target protection-domain
			 * \param utcb_phys  physical UTCB pointer
			 * \param utcb_virt  virtual UTCB pointer
			 */
			void start(void * const        ip,
			           void * const        sp,
			           unsigned const      cpu_id,
			           unsigned const      pd_id,
			           Native_utcb * const utcb_phys,
			           Native_utcb * const utcb_virt);

			/**
			 * Pause this thread
			 */
			void pause();

			/**
			 * Stop this thread
			 */
			void stop();

			/**
			 * Resume this thread
			 */
			int resume();

			/**
			 * Send a request and await the reply
			 */
			void request_and_wait(Thread * const dest, size_t const size);

			/**
			 * Wait for any request
			 */
			void wait_for_request();

			/**
			 * Reply to the last request
			 */
			void reply(size_t const size, bool const await_request);

			/**
			 * Handle a pagefault that originates from this thread
			 *
			 * \param va  virtual fault address
			 * \param w   if fault was caused by a write access
			 */
			void pagefault(addr_t const va, bool const w);

			/**
			 * Get unique thread ID, avoid method ambiguousness
			 */
			unsigned id() const { return Object::id(); }

			/**
			 * Gets called when we await a signal at 'receiver'
			 */
			void await_signal(Kernel::Signal_receiver * receiver);

			/**
			 * Handle the exception that currently blocks this thread
			 */
			void handle_exception();

			/**
			 * Continue executing this thread in userland
			 */
			void proceed();

			void kill_signal_context_blocks();

			void kill_signal_context_done();

			/***************
			 ** Accessors **
			 ***************/

			Platform_thread * platform_thread() const {
				return _platform_thread; }

			void pager(Thread * const p) { _pager = p; }

			unsigned pd_id() const { return _pd_id; }

			Native_utcb * phys_utcb() const { return _phys_utcb; }

			Signal_handler * signal_handler() { return &_signal_handler; }
	};
}

#endif /* _CORE__KERNEL__THREAD_H_ */

