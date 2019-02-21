/*
 * \brief   Kernel backend for execution contexts in userland
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__THREAD_H_
#define _CORE__KERNEL__THREAD_H_

#include <base/signal.h>
#include <util/reconstructible.h>

/* core includes */
#include <cpu.h>
#include <kernel/cpu_context.h>
#include <kernel/inter_processor_work.h>
#include <kernel/signal_receiver.h>
#include <kernel/ipc_node.h>
#include <kernel/object.h>

namespace Kernel
{
	struct Thread_fault;
	class Thread;
	class Core_thread;
}


struct Kernel::Thread_fault
{
	enum Type { WRITE, EXEC, PAGE_MISSING, UNKNOWN };

	addr_t ip    = 0;
	addr_t addr  = 0;
	Type   type  = UNKNOWN;

	void print(Genode::Output &out) const;
};


/**
 * Kernel back-end for userland execution-contexts
 */
class Kernel::Thread
:
	public Kernel::Object, public Cpu_job,
	public Ipc_node, public Signal_context_killer, public Signal_handler,
	private Timeout
{
	private:

		/*
		 * Noncopyable
		 */
		Thread(Thread const &);
		Thread &operator = (Thread const &);

		/**
		 * An update of page-table entries that requires architecture-wise
		 * maintainance operations, e.g., a TLB invalidation needs
		 * cross-cpu synchronization
		 */
		struct Pd_update : Inter_processor_work
		{
			Thread & caller; /* the caller gets blocked until all finished */
			Pd     & pd;     /* the corresponding pd */
			unsigned cnt;    /* count of cpus left */

			Pd_update(Thread & caller, Pd & pd, unsigned cnt);

			/************************************
			 ** Inter_processor_work interface **
			 ************************************/

			void execute() override;
		};

		/**
		 * The destruction of a thread still active on another cpu
		 * needs cross-cpu synchronization
		 */
		struct Destroy : Inter_processor_work
		{
			Thread & caller; /* the caller gets blocked till the end */
			Thread & thread_to_destroy; /* thread to be destroyed */

			Destroy(Thread & caller, Thread & to_destroy);

			/************************************
			 ** Inter_processor_work interface **
			 ************************************/

			void execute() override;
		};

		friend void Pd_update::execute();
		friend void Destroy::execute();

	protected:

		enum { START_VERBOSE = 0 };

		enum State
		{
			ACTIVE                      = 1,
			AWAITS_START                = 2,
			AWAITS_IPC                  = 3,
			AWAITS_RESTART              = 4,
			AWAITS_SIGNAL               = 5,
			AWAITS_SIGNAL_CONTEXT_KILL  = 6,
			DEAD                        = 7,
		};

		Signal_context *   _pager = nullptr;
		Thread_fault       _fault { };
		State              _state;
		Signal_receiver *  _signal_receiver;
		char const * const _label;
		capid_t            _timeout_sigid = 0;
		bool               _paused = false;
		bool               _cancel_next_await_signal = false;
		bool const         _core = false;

		Genode::Constructible<Pd_update> _pd_update {};
		Genode::Constructible<Destroy>   _destroy {};

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
		 * Suspend unrecoverably from execution
		 */
		void _die();

		/**
		 * Handle an exception thrown by the memory management unit
		 */
		void _mmu_exception();

		/**
		 * Handle kernel-call request of the thread
		 */
		void _call();

		/**
		 * Return amount of timer ticks that 'quota' is worth
		 */
		size_t _core_to_kernel_quota(size_t const quota) const;

		void _cancel_blocking();

		bool _restart();


		/*********************************************************
		 ** Kernel-call back-ends, see kernel-interface headers **
		 *********************************************************/

		void _call_new_thread();
		void _call_new_core_thread();
		void _call_thread_quota();
		void _call_start_thread();
		void _call_stop_thread();
		void _call_pause_thread();
		void _call_resume_thread();
		void _call_cancel_thread_blocking();
		void _call_restart_thread();
		void _call_yield_thread();
		void _call_delete_thread();
		void _call_await_request_msg();
		void _call_send_request_msg();
		void _call_send_reply_msg();
		void _call_update_pd();
		void _call_update_data_region();
		void _call_update_instr_region();
		void _call_print_char();
		void _call_await_signal();
		void _call_cancel_next_await_signal();
		void _call_submit_signal();
		void _call_ack_signal();
		void _call_kill_signal_context();
		void _call_new_vm();
		void _call_delete_vm();
		void _call_run_vm();
		void _call_pause_vm();
		void _call_pager();
		void _call_new_irq();
		void _call_ack_irq();
		void _call_new_obj();
		void _call_delete_obj();
		void _call_ack_cap();
		void _call_delete_cap();
		void _call_timeout();
		void _call_timeout_max_us();
		void _call_time();

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

		void _signal_context_kill_pending() override;
		void _signal_context_kill_failed()  override;
		void _signal_context_kill_done()    override;


		/********************
		 ** Signal_handler **
		 ********************/

		void _await_signal(Signal_receiver * const receiver) override;
		void _receive_signal(void * const base, size_t const size) override;


		/**************
		 ** Ipc_node **
		 **************/

		void _send_request_succeeded()  override;
		void _send_request_failed()     override;
		void _await_request_succeeded() override;
		void _await_request_failed()    override;

	public:

		Genode::Align_at<Genode::Cpu::Context> regs;

		/**
		 * Constructor
		 *
		 * \param priority  scheduling priority
		 * \param quota     CPU-time quota
		 * \param label     debugging label
		 * \param core      whether it is a core thread or not
		 */
		Thread(unsigned const priority, unsigned const quota,
		       char const * const label, bool core = false);

		/**
		 * Constructor for core/kernel thread
		 *
		 * \param label  debugging label
		 */
		Thread(char const * const label)
		: Thread(Cpu_priority::MIN, 0, label, true) { }


		/**************************
		 ** Support for syscalls **
		 **************************/

		void user_ret_time(Kernel::time_t const t);

		void user_arg_0(Kernel::Call_arg const arg);
		void user_arg_1(Kernel::Call_arg const arg);
		void user_arg_2(Kernel::Call_arg const arg);
		void user_arg_3(Kernel::Call_arg const arg);
		void user_arg_4(Kernel::Call_arg const arg);

		Kernel::Call_arg user_arg_0() const;
		Kernel::Call_arg user_arg_1() const;
		Kernel::Call_arg user_arg_2() const;
		Kernel::Call_arg user_arg_3() const;
		Kernel::Call_arg user_arg_4() const;

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
		 * Syscall to create a core thread
		 *
		 * \param p         memory donation for the new kernel thread object
		 * \param label     debugging label of the new thread
		 *
		 * \retval capability id of the new kernel object
		 */
		static capid_t syscall_create(void * const p, char const * const label)
		{
			return call(call_id_new_core_thread(), (Call_arg)p,
			            (Call_arg)label);
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

		void exception(Cpu & cpu) override;
		void proceed(Cpu & cpu)   override;
		Cpu_job * helping_sink()  override;


		/*************
		 ** Timeout **
		 *************/

		void timeout_triggered() override;


		/***************
		 ** Accessors **
		 ***************/

		char const * label() const { return _label; }
		Thread_fault fault() const { return _fault; }
};


/**
 * The first core thread in the system bootstrapped by the Kernel
 */
struct Kernel::Core_thread : Core_object<Kernel::Thread>
{
	Core_thread();

	static Thread & singleton();
};

#endif /* _CORE__KERNEL__THREAD_H_ */
