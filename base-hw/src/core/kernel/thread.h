/*
 * \brief   Kernel backend for userland execution-contexts
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
#include <kernel/configuration.h>
#include <kernel/scheduler.h>
#include <kernel/signal_receiver.h>
#include <kernel/ipc_node.h>
#include <kernel/irq_receiver.h>
#include <kernel/pd.h>
#include <cpu.h>

namespace Genode
{
	class Platform_thread;
}

namespace Kernel
{
	class Thread;

	typedef Genode::Cpu         Cpu;
	typedef Genode::Pagefault   Pagefault;
	typedef Genode::Native_utcb Native_utcb;

	unsigned core_id();
	void     handle_syscall(Thread * const);
	void     handle_interrupt(void);
	void     handle_invalid_excpt(void);

	/**
	 * Kernel object that can be scheduled for the CPU
	 */
	class Execution_context;

	typedef Scheduler<Execution_context> Cpu_scheduler;

	/**
	 * Return the systems CPU scheduler
	 */
	static Cpu_scheduler * cpu_scheduler();

	/**
	 * Kernel backend for userland execution-contexts
	 */
	class Thread;
}

class Kernel::Execution_context : public Cpu_scheduler::Item
{
	public:

		/**
		 * Handle an exception that occured during execution
		 */
		virtual void handle_exception() = 0;

		/**
		 * Continue execution
		 */
		virtual void proceed() = 0;

		/**
		 * Destructor
		 */
		virtual ~Execution_context()
		{
			if (list()) { cpu_scheduler()->remove(this); }
		}
};

class Kernel::Thread
:
	public Cpu::User_context,
	public Object<Thread, MAX_THREADS>,
	public Execution_context,
	public Ipc_node,
	public Irq_receiver,
	public Signal_context_killer,
	public Signal_receiver_killer,
	public Signal_handler
{
	private:

		enum State
		{
			SCHEDULED                  = 1,
			AWAIT_START                = 2,
			AWAIT_IPC                  = 3,
			AWAIT_RESUME               = 4,
			AWAIT_PAGER                = 5,
			AWAIT_PAGER_IPC            = 6,
			AWAIT_IRQ                  = 7,
			AWAIT_SIGNAL               = 8,
			AWAIT_SIGNAL_CONTEXT_KILL  = 9,
			AWAIT_SIGNAL_RECEIVER_KILL = 10,
			STOPPED                    = 11,
		};

		Platform_thread * const _platform_thread;
		State                   _state;
		Pagefault               _pagefault;
		Thread *                _pager;
		unsigned                _pd_id;
		Native_utcb *           _phys_utcb;
		Native_utcb *           _virt_utcb;
		Signal_receiver *       _signal_receiver;

		/**
		 * Resume execution
		 */
		void _schedule()
		{
			cpu_scheduler()->insert(this);
			_state = SCHEDULED;
		}


		/***************************
		 ** Signal_context_killer **
		 ***************************/

		void _signal_context_kill_pending()
		{
			assert(_state == SCHEDULED);
			_state = AWAIT_SIGNAL_CONTEXT_KILL;
			cpu_scheduler()->remove(this);
		}

		void _signal_context_kill_done()
		{
			assert(_state == AWAIT_SIGNAL_CONTEXT_KILL);
			user_arg_0(0);
			_schedule();
		}


		/****************************
		 ** Signal_receiver_killer **
		 ****************************/

		void _signal_receiver_kill_pending()
		{
			assert(_state == SCHEDULED);
			_state = AWAIT_SIGNAL_RECEIVER_KILL;
			cpu_scheduler()->remove(this);
		}

		void _signal_receiver_kill_done()
		{
			assert(_state == AWAIT_SIGNAL_RECEIVER_KILL);
			user_arg_0(0);
			_schedule();
		}


		/********************
		 ** Signal_handler **
		 ********************/

		void _await_signal(Signal_receiver * const receiver)
		{
			cpu_scheduler()->remove(this);
			_state = AWAIT_SIGNAL;
			_signal_receiver = receiver;
		}

		void _receive_signal(void * const base, size_t const size)
		{
			assert(_state == AWAIT_SIGNAL && size <= phys_utcb()->size());
			Genode::memcpy(phys_utcb()->base(), base, size);
			_schedule();
		}


		/**************
		 ** Ipc_node **
		 **************/

		void _received_ipc_request(size_t const s)
		{
			switch (_state) {
			case SCHEDULED:
				user_arg_0(s);
				return;
			default:
				PERR("wrong thread state to receive IPC");
				stop();
				return;
			}
		}

		void _await_ipc()
		{
			switch (_state) {
			case SCHEDULED:
				cpu_scheduler()->remove(this);
				_state = AWAIT_IPC;
			case AWAIT_PAGER:
				return;
			default:
				PERR("wrong thread state to await IPC");
				stop();
				return;
			}
		}

		void _await_ipc_succeeded(size_t const s)
		{
			switch (_state) {
			case AWAIT_IPC:
				_schedule();
				user_arg_0(s);
				return;
			case AWAIT_PAGER_IPC:
				_schedule();
				return;
			case AWAIT_PAGER:
				_state = AWAIT_RESUME;
				return;
			default:
				PERR("wrong thread state to receive IPC");
				stop();
				return;
			}
		}

		void _await_ipc_failed()
		{
			switch (_state) {
			case AWAIT_IPC:
				PERR("failed to receive IPC");
				stop();
				return;
			case SCHEDULED:
				PERR("failed to receive IPC");
				stop();
				return;
			case AWAIT_PAGER_IPC:
				PERR("failed to get pagefault resolved");
				stop();
				return;
			case AWAIT_PAGER:
				PERR("failed to get pagefault resolved");
				stop();
				return;
			default:
				PERR("wrong thread state to cancel IPC");
				stop();
				return;
			}
		}


		/***************
		 ** Irq_owner **
		 ***************/

		void _received_irq()
		{
			assert(_state == AWAIT_IRQ);
			_schedule();
		}

		void _awaits_irq()
		{
			cpu_scheduler()->remove(this);
			_state = AWAIT_IRQ;
		}

	public:

		/**
		 * Constructor
		 *
		 * \param platform_thread  userland backend of execution context
		 */
		Thread(Platform_thread * const platform_thread)
		:
			_platform_thread(platform_thread), _state(AWAIT_START),
			_pager(0), _pd_id(0), _phys_utcb(0), _virt_utcb(0),
			_signal_receiver(0)
		{ }

		/**
		 * Suspend the thread unrecoverably
		 */
		void stop()
		{
			if (_state == SCHEDULED) { cpu_scheduler()->remove(this); }
			_state = STOPPED;
		}

		/**
		 * Prepare thread to get scheduled the first time
		 *
		 * \param ip         initial instruction pointer
		 * \param sp         initial stack pointer
		 * \param cpu_id     target cpu
		 * \param pd_id      target protection-domain
		 * \param utcb_phys  physical UTCB pointer
		 * \param utcb_virt  virtual UTCB pointer
		 * \param main       wether the thread is the first one in its PD
		 */
		void prepare_to_start(void * const        ip,
		                      void * const        sp,
		                      unsigned const      cpu_id,
		                      unsigned const      pd_id,
		                      Native_utcb * const utcb_phys,
		                      Native_utcb * const utcb_virt,
		                      bool const          main)
		{
			assert(_state == AWAIT_START)

			/* FIXME: support SMP */
			if (cpu_id) { PERR("multicore processing not supported"); }

			/* store thread parameters */
			_phys_utcb = utcb_phys;
			_virt_utcb = utcb_virt;
			_pd_id     = pd_id;

			/* join a protection domain */
			Pd * const pd = Pd::pool()->object(_pd_id);
			assert(pd)
			addr_t const tlb = pd->tlb()->base();

			/* initialize CPU context */
			User_context * const c = static_cast<User_context *>(this);
			bool const core = (_pd_id == core_id());
			if (!main) { c->init_thread(ip, sp, tlb, pd_id); }
			else if (!core) { c->init_main_thread(ip, utcb_virt, tlb, pd_id); }
			else { c->init_core_main_thread(ip, sp, tlb, pd_id); }
		}

		/**
		 * Start this thread
		 *
		 * \param ip      initial instruction pointer
		 * \param sp      initial stack pointer
		 * \param cpu_id  target cpu
		 * \param pd_id   target protection-domain
		 * \param utcb_p  physical UTCB pointer
		 * \param utcb_v  virtual UTCB pointer
		 * \param main    wether the thread is the first one in its PD
		 */
		void start(void * const        ip,
		           void * const        sp,
		           unsigned const      cpu_id,
		           unsigned const      pd_id,
		           Native_utcb * const utcb_p,
		           Native_utcb * const utcb_v,
		           bool const          main)
		{
			prepare_to_start(ip, sp, cpu_id, pd_id, utcb_p, utcb_v, main);
			_schedule();
		}

		/**
		 * Pause this thread
		 */
		void pause()
		{
			assert(_state == AWAIT_RESUME || _state == SCHEDULED);
			cpu_scheduler()->remove(this);
			_state = AWAIT_RESUME;
		}

		/**
		 * Resume this thread
		 */
		int resume()
		{
			switch (_state) {
			case AWAIT_RESUME:
				_schedule();
				return 0;
			case AWAIT_PAGER:
				_state = AWAIT_PAGER_IPC;
				return 0;
			case AWAIT_PAGER_IPC:
				PERR("cancel message receipt");
				Ipc_node::cancel_waiting();
				return 0;
			case SCHEDULED:
				return 1;
			case AWAIT_IPC:
				PERR("cancel message receipt");
				Ipc_node::cancel_waiting();
				_schedule();
				return 0;
			case AWAIT_IRQ:
				PERR("cancel interrupt receipt");
				Irq_receiver::cancel_waiting();
				_schedule();
				return 0;
			case AWAIT_SIGNAL:
				PERR("cancel signal receipt");
				Signal_handler::cancel_waiting();
				_schedule();
				return 0;
			case AWAIT_SIGNAL_CONTEXT_KILL:
				PERR("cancel signal context destruction");
				Signal_context_killer::cancel_waiting();
				_schedule();
				return 0;
			case AWAIT_SIGNAL_RECEIVER_KILL:
				PERR("cancel signal receiver destruction");
				Signal_receiver_killer::cancel_waiting();
				_schedule();
				return 0;
			case AWAIT_START:
			case STOPPED:;
			}
			PERR("failed to resume thread");
			return -1;
		}

		/**
		 * Send a request and await the reply
		 */
		void request_and_wait(Thread * const dest, size_t const size)
		{
			Ipc_node::send_request_await_reply(
				dest, phys_utcb()->base(), size, phys_utcb()->base(),
				phys_utcb()->size());
		}

		/**
		 * Wait for any request
		 */
		void wait_for_request()
		{
			Ipc_node::await_request(phys_utcb()->base(), phys_utcb()->size());
		}

		/**
		 * Reply to the last request
		 */
		void reply(size_t const size, bool const await_request)
		{
			Ipc_node::send_reply(phys_utcb()->base(), size);
			if (await_request) {
				Ipc_node * const ipc = static_cast<Ipc_node *>(this);
				ipc->await_request(phys_utcb()->base(), phys_utcb()->size());
			}
			else { user_arg_0(0); }
		}

		/**
		 * Handle an exception thrown by the MMU
		 */
		void handle_mmu_exception()
		{
			/* pause thread */
			cpu_scheduler()->remove(this);
			_state = AWAIT_PAGER;

			/* check out cause and attributes */
			addr_t va = 0;
			bool   w  = 0;
			if (!pagefault(va, w)) {
				PERR("unknown MMU exception");
				return;
			}
			/* inform pager */
			_pagefault = Pagefault(id(), (Tlb *)tlb(), ip, va, w);
			void * const base = &_pagefault;
			size_t const size = sizeof(_pagefault);
			Ipc_node::send_request_await_reply(_pager, base, size, base, size);
		}

		/**
		 * Get unique thread ID, avoid method ambiguousness
		 */
		unsigned id() const { return Object::id(); }


		/***********************
		 ** Execution_context **
		 ***********************/

		void handle_exception()
		{
			switch(cpu_exception) {
			case SUPERVISOR_CALL:
				handle_syscall(this);
				return;
			case PREFETCH_ABORT:
				handle_mmu_exception();
				return;
			case DATA_ABORT:
				handle_mmu_exception();
				return;
			case INTERRUPT_REQUEST:
				handle_interrupt();
				return;
			case FAST_INTERRUPT_REQUEST:
				handle_interrupt();
				return;
			default:
				handle_invalid_excpt();
			}
		}

		void proceed()
		{
			mtc()->continue_user(static_cast<Cpu::Context *>(this));
		}


		/***************
		 ** Accessors **
		 ***************/

		Platform_thread * platform_thread() const { return _platform_thread; }

		void pager(Thread * const p) { _pager = p; }

		unsigned pd_id() const { return _pd_id; }

		Native_utcb * phys_utcb() const { return _phys_utcb; }
};

#endif /* _CORE__KERNEL__THREAD_H_ */
