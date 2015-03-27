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
#include <kernel/signal_receiver.h>
#include <kernel/ipc_node.h>
#include <kernel/cpu.h>
#include <kernel/thread_base.h>

namespace Kernel
{
	class Thread;
	class Pd;

	typedef Genode::Native_utcb Native_utcb;

	/**
	 * Kernel backend for userland execution-contexts
	 */
	class Thread;

	typedef Object_pool<Thread> Thread_pool;

	Thread_pool * thread_pool();
}

class Kernel::Thread
:
	public Cpu::User_context,
	public Object<Thread, thread_pool>,
	public Cpu_domain_update, public Ipc_node, public Signal_context_killer,
	public Signal_handler, public Thread_base, public Cpu_job
{
	friend class Thread_event;

	private:

		enum { START_VERBOSE = 0 };

		enum State
		{
			ACTIVE                      = 1,
			AWAITS_START                = 2,
			AWAITS_IPC                  = 3,
			AWAITS_RESUME               = 4,
			AWAITS_SIGNAL               = 5,
			AWAITS_SIGNAL_CONTEXT_KILL  = 6,
			STOPPED                     = 7,
		};

		State              _state;
		Pd *               _pd;
		Native_utcb *      _utcb_phys;
		Signal_receiver *  _signal_receiver;
		char const * const _label;

		/**
		 * Notice that another thread yielded the CPU to this thread
		 */
		void _receive_yielded_cpu();

		/**
		 * Attach or detach the handler of a thread-triggered event
		 *
		 * \param event_id           kernel name of the thread event
		 * \param signal_context_id  kernel name signal context or 0 to detach
		 *
		 * \retval  0  succeeded
		 * \retval -1  failed
		 */
		int _route_event(unsigned const event_id,
		                 unsigned const signal_context_id);

		/**
		 * Map kernel name of thread event to the corresponding member
		 *
		 * \param id  kernel name of targeted thread event
		 *
		 * \retval  0  failed
		 * \retval >0  targeted member pointer
		 */
		Thread_event Thread::* _event(unsigned const id) const;

		/**
		 * Return wether this is a core thread
		 */
		bool _core() const;

		/**
		 * Switch from an inactive state to the active state
		 */
		void _become_active();

		/**
		 * Switch from the active state to the inactive state 's'
		 */
		void _become_inactive(State const s);

		/**
		 * Activate our CPU-share and those of our helpers
		 */
		void _activate_used_shares();

		/**
		 * Deactivate our CPU-share and those of our helpers
		 */
		void _deactivate_used_shares();

		/**
		 * Pause execution
		 */
		void _pause();

		/**
		 * Suspend unrecoverably from execution
		 */
		void _stop();

		/**
		 * Cancel blocking if possible
		 *
		 * \return  wether thread was in a cancelable blocking beforehand
		 */
		bool _resume();

		/**
		 * Handle an exception thrown by the memory management unit
		 */
		void _mmu_exception();

		/**
		 * Handle kernel-call request of the thread
		 */
		void _call();

		/**
		 * Read a thread register
		 *
		 * \param id     kernel name of targeted thread register
		 * \param value  read-value buffer
		 *
		 * \retval  0  succeeded
		 * \retval -1  failed
		 */
		int _read_reg(addr_t const id, addr_t & value) const;

		/**
		 * Return amount of timer tics that 'quota' is worth 
		 */
		size_t _core_to_kernel_quota(size_t const quota) const;

		/**
		 * Override a thread register
		 *
		 * \param id     kernel name of targeted thread register
		 * \param value  write-value buffer
		 *
		 * \retval  0  succeeded
		 * \retval -1  failed
		 */
		int _write_reg(addr_t const id, addr_t const value);

		/**
		 * Map kernel names of thread registers to the corresponding data
		 *
		 * \param id  kernel name of thread register
		 *
		 * \retval  0  failed
		 * \retval >0  pointer to register content
		 */
		addr_t Thread::* _reg(addr_t const id) const;

		/**
		 * Print table of all threads and their current activity
		 */
		void _print_activity_table();

		/**
		 * Print the activity of the thread
		 *
		 * \param printing_thread  wether this thread caused the debugging
		 */
		void _print_activity(bool const printing_thread);

		/**
		 * Print the activity of the thread when it awaits a message
		 */
		void _print_activity_when_awaits_ipc();

		/**
		 * Print activity info that is printed regardless of the thread state
		 */
		void _print_common_activity();


		/*********************************************************
		 ** Kernel-call back-ends, see kernel-interface headers **
		 *********************************************************/

		void _call_new_pd();
		void _call_delete_pd();
		void _call_new_thread();
		void _call_thread_quota();
		void _call_delete_thread();
		void _call_start_thread();
		void _call_pause_current_thread();
		void _call_pause_thread();
		void _call_resume_thread();
		void _call_resume_local_thread();
		void _call_yield_thread();
		void _call_await_request_msg();
		void _call_send_request_msg();
		void _call_send_reply_msg();
		void _call_update_pd();
		void _call_update_data_region();
		void _call_update_instr_region();
		void _call_print_char();
		void _call_new_signal_receiver();
		void _call_new_signal_context();
		void _call_await_signal();
		void _call_signal_pending();
		void _call_submit_signal();
		void _call_ack_signal();
		void _call_kill_signal_context();
		void _call_delete_signal_context();
		void _call_delete_signal_receiver();
		void _call_new_vm();
		void _call_delete_vm();
		void _call_run_vm();
		void _call_pause_vm();
		void _call_access_thread_regs();
		void _call_route_thread_event();
		void _call_new_irq();
		void _call_delete_irq();


		/***************************
		 ** Signal_context_killer **
		 ***************************/

		void _signal_context_kill_pending();
		void _signal_context_kill_failed();
		void _signal_context_kill_done();


		/********************
		 ** Signal_handler **
		 ********************/

		void _await_signal(Signal_receiver * const receiver);
		void _receive_signal(void * const base, size_t const size);


		/**************
		 ** Ipc_node **
		 **************/

		void _send_request_succeeded();
		void _send_request_failed();
		void _await_request_succeeded();
		void _await_request_failed();


		/***********************
		 ** Cpu_domain_update **
		 ***********************/

		void _cpu_domain_update_unblocks() { _resume(); }

	public:

		/**
		 * Constructor
		 *
		 * \param priority  scheduling priority
		 * \param quota     CPU-time quota
		 * \param label     debugging label
		 */
		Thread(unsigned const priority, unsigned const quota,
		       char const * const label);

		/**
		 * Prepare thread to get active the first time
		 *
		 * \param cpu    targeted CPU
		 * \param pd     targeted domain
		 * \param utcb   core local pointer to userland thread-context
		 * \param start  wether to start executing the thread
		 */
		void init(Cpu * const cpu, Pd * const pd, Native_utcb * const utcb,
		          bool const start);


		/*************
		 ** Cpu_job **
		 *************/

		void exception(unsigned const cpu);
		void proceed(unsigned const cpu);
		Cpu_job * helping_sink();


		/***************
		 ** Accessors **
		 ***************/

		unsigned     id() const    { return Object::id(); }
		char const * label() const { return _label;       }
		char const * pd_label() const;
		Pd * const   pd() const    { return _pd;          }
};

#endif /* _KERNEL__THREAD_H_ */
