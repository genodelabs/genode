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

#ifndef _CORE__INCLUDE__KERNEL__THREAD_H_
#define _CORE__INCLUDE__KERNEL__THREAD_H_

/* core includes */
#include <kernel/signal_receiver.h>
#include <kernel/ipc_node.h>
#include <kernel/cpu.h>
#include <kernel/object.h>
#include <base/signal.h>

namespace Kernel
{
	class Thread;
	class Thread_event;
	class Core_thread;
}

/**
 * Event that is provided by kernel thread-objects for user handling
 */
class Kernel::Thread_event : public Signal_ack_handler
{
	private:

		Thread * const   _thread;
		Signal_context * _signal_context;


		/************************
		 ** Signal_ack_handler **
		 ************************/

		void _signal_acknowledged();

	public:

		/**
		 * Constructor
		 *
		 * \param t  thread that blocks on the event
		 */
		Thread_event(Thread * const t);

		/**
		 * Submit to listening handlers just like a signal context
		 */
		void submit();

		/**
		 * Kernel name of assigned signal context or 0 if not assigned
		 */
		Signal_context * const signal_context() const;

		/**
		 * Override signal context of the event
		 *
		 * \param c  new signal context or 0 to dissolve current signal context
		 */
		void signal_context(Signal_context * const c);
};

/**
 * Kernel back-end for userland execution-contexts
 */
class Kernel::Thread
:
	public Kernel::Object, public Cpu_job, public Cpu_domain_update,
	public Ipc_node, public Signal_context_killer, public Signal_handler,
	private Timeout
{
	friend class Thread_event;
	friend class Core_thread;

	private:

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

		Thread_event       _fault;
		addr_t             _fault_pd;
		addr_t             _fault_addr;
		addr_t             _fault_writes;
		addr_t             _fault_signal;
		State              _state;
		Signal_receiver *  _signal_receiver;
		char const * const _label;
		capid_t            _timeout_sigid = 0;

		void _init();

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
		int _route_event(unsigned         const event_id,
		                 Signal_context * const signal_context_id);

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
		 * Return amount of timer tics that 'quota' is worth
		 */
		size_t _core_to_kernel_quota(size_t const quota) const;


		/*********************************************************
		 ** Kernel-call back-ends, see kernel-interface headers **
		 *********************************************************/

		void _call_new_thread();
		void _call_thread_quota();
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
		void _call_await_signal();
		void _call_submit_signal();
		void _call_ack_signal();
		void _call_kill_signal_context();
		void _call_new_vm();
		void _call_delete_vm();
		void _call_run_vm();
		void _call_pause_vm();
		void _call_route_thread_event();
		void _call_new_irq();
		void _call_ack_irq();
		void _call_new_obj();
		void _call_delete_obj();
		void _call_ack_cap();
		void _call_delete_cap();
		void _call_timeout();
		void _call_timeout_age_us();
		void _call_timeout_max_us();

		template <typename T, typename... ARGS>
		void _call_new(ARGS &&... args)
		{
			using Object = Core_object<T>;
			void * dst = (void *)user_arg_1();
			Object * o = Genode::construct_at<Object>(dst, args...);
			user_arg_0(o->core_capid());
		}


		template <typename T>
		void _call_delete()
		{
			using Object = Core_object<T>;
			reinterpret_cast<Object*>(user_arg_1())->~Object();
		}


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
		 * Syscall to create a thread
		 *
		 * \param p         memory donation for the new kernel thread object
		 * \param priority  scheduling priority of the new thread
		 * \param quota     CPU quota of the new thread
		 * \param label     debugging label of the new thread
		 *
		 * \retval capability id of the new kernel object
		 */
		static capid_t syscall_create(void * const p, unsigned const priority,
		                              size_t const quota,
		                              char const * const label)
		{
			return call(call_id_new_thread(), (Call_arg)p, (Call_arg)priority,
			            (Call_arg)quota, (Call_arg)label);
		}

		/**
		 * Syscall to destroy a thread
		 *
		 * \param thread  pointer to thread kernel object
		 */
		static void syscall_destroy(Thread * thread) {
			call(call_id_delete_thread(), (Call_arg)thread); }

		void print(Genode::Output &out) const;


		/*************
		 ** Cpu_job **
		 *************/

		void exception(unsigned const cpu);
		void proceed(unsigned const cpu);
		Cpu_job * helping_sink();


		/*************
		 ** Timeout **
		 *************/

		void timeout_triggered();


		/***************
		 ** Accessors **
		 ***************/

		char const * label()  const { return _label; }
		addr_t fault_pd()     const { return _fault_pd; }
		addr_t fault_addr()   const { return _fault_addr; }
		addr_t fault_writes() const { return _fault_writes; }
		addr_t fault_signal() const { return _fault_signal; }
};


/**
 * The first core thread in the system bootstrapped by the Kernel
 */
class Kernel::Core_thread : public Core_object<Kernel::Thread>
{
	private:

		Core_thread();

	public:

		static Thread & singleton();
};

#endif /* _CORE__INCLUDE__KERNEL__THREAD_H_ */
