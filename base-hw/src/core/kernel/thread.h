/*
 * \brief   Kernel backend for execution contexts in userland
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__THREAD_H_
#define _KERNEL__THREAD_H_

/* core includes */
#include <kernel/configuration.h>
#include <kernel/scheduler.h>
#include <kernel/signal_receiver.h>
#include <kernel/ipc_node.h>
#include <kernel/irq_receiver.h>
#include <cpu.h>

namespace Genode
{
	class Platform_thread;
}

namespace Kernel
{
	class Thread;
	class Pd;

	typedef Genode::Cpu         Cpu;
	typedef Genode::Pagefault   Pagefault;
	typedef Genode::Native_utcb Native_utcb;

	void reset_lap_time();

	/**
	 * Kernel backend for userland execution-contexts
	 */
	class Thread;

	class Thread_ids : public Id_allocator<MAX_THREADS> { };
	typedef Object_pool<Thread> Thread_pool;

	Thread_ids  * thread_ids();
	Thread_pool * thread_pool();
}

class Kernel::Thread
:
	public Cpu::User_context,
	public Object<Thread, MAX_THREADS, Thread_ids, thread_ids, thread_pool>,
	public Execution_context,
	public Ipc_node,
	public Irq_receiver,
	public Signal_context_killer,
	public Signal_receiver_killer,
	public Signal_handler
{
	private:

		enum { START_VERBOSE = 0 };

		enum State
		{
			SCHEDULED                   = 1,
			AWAITS_START                = 2,
			AWAITS_IPC                  = 3,
			AWAITS_RESUME               = 4,
			AWAITS_PAGER                = 5,
			AWAITS_PAGER_IPC            = 6,
			AWAITS_IRQ                  = 7,
			AWAITS_SIGNAL               = 8,
			AWAITS_SIGNAL_CONTEXT_KILL  = 9,
			AWAITS_SIGNAL_RECEIVER_KILL = 10,
			STOPPED                     = 11,
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
		 * Notice that another thread yielded the CPU to this thread
		 */
		void _receive_yielded_cpu();

		/**
		 * Return kernel backend of protection domain the thread is in
		 */
		Pd * _pd() const;

		/**
		 * Return wether this is a core thread
		 */
		bool _core() const;

		/**
		 * Resume execution rawly
		 */
		void _schedule();

		/**
		 * Pause execution
		 */
		void _pause();

		/**
		 * Suspend unrecoverably from execution
		 */
		void _stop();

		/**
		 * Try to escape from blocking state, if in any, and resume execution
		 *
		 * \retval -1  failed
		 * \retval  0  succeeded, execution was paused
		 * \retval  1  succeeded, execution was not paused
		 */
		int _resume();

		/**
		 * Handle an exception thrown by the memory management unit
		 */
		void _mmu_exception();

		/**
		 * Handle syscall request of this thread
		 */
		void _syscall();


		/***************************************************
		 ** Syscall backends, for details see 'syscall.h' **
		 ***************************************************/

		void _syscall_new_pd();
		void _syscall_kill_pd();
		void _syscall_new_thread();
		void _syscall_delete_thread();
		void _syscall_start_thread();
		void _syscall_pause_thread();
		void _syscall_resume_thread();
		void _syscall_resume_faulter();
		void _syscall_yield_thread();
		void _syscall_current_thread_id();
		void _syscall_get_thread();
		void _syscall_wait_for_request();
		void _syscall_request_and_wait();
		void _syscall_reply();
		void _syscall_set_pager();
		void _syscall_update_pd();
		void _syscall_update_region();
		void _syscall_allocate_irq();
		void _syscall_free_irq();
		void _syscall_await_irq();
		void _syscall_print_char();
		void _syscall_read_thread_state();
		void _syscall_write_thread_state();
		void _syscall_new_signal_receiver();
		void _syscall_new_signal_context();
		void _syscall_await_signal();
		void _syscall_signal_pending();
		void _syscall_submit_signal();
		void _syscall_ack_signal();
		void _syscall_kill_signal_context();
		void _syscall_kill_signal_receiver();
		void _syscall_new_vm();
		void _syscall_run_vm();
		void _syscall_pause_vm();


		/***************************
		 ** Signal_context_killer **
		 ***************************/

		void _signal_context_kill_pending();
		void _signal_context_kill_done();


		/****************************
		 ** Signal_receiver_killer **
		 ****************************/

		void _signal_receiver_kill_pending();
		void _signal_receiver_kill_done();


		/********************
		 ** Signal_handler **
		 ********************/

		void _await_signal(Signal_receiver * const receiver);
		void _receive_signal(void * const base, size_t const size);


		/**************
		 ** Ipc_node **
		 **************/

		void _received_ipc_request(size_t const s);
		void _await_ipc();
		void _await_ipc_succeeded(size_t const s);
		void _await_ipc_failed();


		/***************
		 ** Irq_owner **
		 ***************/

		void _received_irq();
		void _awaits_irq();

	public:

		/**
		 * Constructor
		 *
		 * \param platform_thread  corresponding userland object
		 */
		Thread(Platform_thread * const platform_thread);

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
		 * \param start      wether to start execution
		 */
		void init(void * const ip, void * const sp, unsigned const cpu_id,
		          unsigned const pd_id, Native_utcb * const utcb_phys,
		          Native_utcb * const utcb_virt, bool const main,
		          bool const start);


		/***********************
		 ** Execution_context **
		 ***********************/

		void handle_exception();
		void proceed();


		/***************
		 ** Accessors **
		 ***************/

		Platform_thread * platform_thread() const { return _platform_thread; }
		void              pager(Thread * const p) { _pager = p; }
		unsigned          id() const { return Object::id(); }
		char const *      label() const;
		unsigned          pd_id() const { return _pd_id; }
		char const *      pd_label() const;
};

#endif /* _KERNEL__THREAD_H_ */
