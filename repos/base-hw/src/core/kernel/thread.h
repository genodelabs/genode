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

/* Genode includes */
#include <base/signal.h>

/* base-hw core includes */
#include <kernel/cpu_context.h>
#include <kernel/inter_processor_work.h>
#include <kernel/signal.h>
#include <kernel/ipc_node.h>
#include <object.h>
#include <kernel/interface.h>
#include <assertion.h>

/* base internal includes */
#include <base/internal/native_utcb.h>

namespace Kernel {

	class Cpu_pool;
	struct Thread_fault;
	class Thread;
	class Core_main_thread;
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
class Kernel::Thread : private Kernel::Object, public Cpu_context, private Timeout
{
	public:

		enum Type { USER, CORE, IDLE };

		enum Exception_state { NO_EXCEPTION, MMU_FAULT, EXCEPTION };

	private:

		/*
		 * Noncopyable
		 */
		Thread(Thread const &);
		Thread &operator = (Thread const &);

		/**
		 * A TLB invalidation may need cross-cpu synchronization
		 */
		struct Tlb_invalidation : Inter_processor_work
		{
			Inter_processor_work_list &global_work_list;
			Thread                    &caller; /* the caller gets blocked until all finished */
			Pd                        &pd;     /* the corresponding pd */
			addr_t                     addr;
			size_t                     size;
			unsigned                   cnt;    /* count of cpus left */

			Tlb_invalidation(Inter_processor_work_list &global_work_list,
			                 Thread                    &caller,
			                 Pd                        &pd,
			                 addr_t                     addr,
			                 size_t                     size,
			                 unsigned                   cnt);

			/************************************
			 ** Inter_processor_work interface **
			 ************************************/

			void execute(Cpu &) override;
		};

		/**
		 * The destruction of a thread still active on another cpu
		 * needs cross-cpu synchronization
		 */
		struct Destroy : Inter_processor_work
		{
			using Kthread = Core::Kernel_object<Thread>;

			Thread  & caller; /* the caller gets blocked till the end */
			Kthread & thread_to_destroy; /* thread to be destroyed */

			Destroy(Thread & caller, Kthread & to_destroy);

			/************************************
			 ** Inter_processor_work interface **
			 ************************************/

			void execute(Cpu &) override;
		};

		/**
		 * Flush and stop CPU, e.g. before suspending or powering off the CPU
		 */
		struct Flush_and_stop_cpu : Inter_processor_work
		{
			Inter_processor_work_list &global_work_list;
			unsigned                   cpus_left;
			Hw::Suspend_type           suspend;

			Flush_and_stop_cpu(Inter_processor_work_list &global_work_list,
			                   unsigned cpus, Hw::Suspend_type suspend)
			:
				global_work_list(global_work_list),
				cpus_left(cpus),
				suspend(suspend)
			{
				global_work_list.insert(&_le);
			}

			~Flush_and_stop_cpu() { global_work_list.remove(&_le); }

			/************************************
			 ** Inter_processor_work interface **
			 ************************************/

			void execute(Cpu &) override;
		};

		friend void Tlb_invalidation::execute(Cpu &);
		friend void Destroy::execute(Cpu &);
		friend void Flush_and_stop_cpu::execute(Cpu &);

	protected:

		enum { START_VERBOSE = 0 };

		enum State {
			ACTIVE                      = 1,
			AWAITS_START                = 2,
			AWAITS_IPC                  = 3,
			AWAITS_RESTART              = 4,
			AWAITS_SIGNAL               = 5,
			AWAITS_SIGNAL_CONTEXT_KILL  = 6,
			DEAD                        = 7,
		};

		enum { MAX_RCV_CAPS = Genode::Msgbuf_base::MAX_CAPS_PER_MSG };

		Board::Address_space_id_allocator &_addr_space_id_alloc;
		Irq::Pool                         &_user_irq_pool;
		Cpu_pool                          &_cpu_pool;
		Pd                                &_core_pd;
		void                              *_obj_id_ref_ptr[MAX_RCV_CAPS] { nullptr };
		Ipc_node                           _ipc_node;
		capid_t                            _ipc_capid                { cap_id_invalid() };
		size_t                             _ipc_rcv_caps             { 0 };
		Genode::Native_utcb               *_utcb                     { nullptr };
		Pd                                *_pd                       { nullptr };

		struct Fault_context
		{
			Thread         &pager;
			Signal_context &sc;
		};

		Genode::Constructible<Fault_context> _fault_context {};

		Thread_fault                       _fault                    { };
		State                              _state;
		Signal_handler                     _signal_handler           { *this };
		Signal_context_killer              _signal_context_killer    { *this };
		char   const *const                _label;
		capid_t                            _timeout_sigid            { 0 };
		bool                               _paused                   { false };
		bool                               _cancel_next_await_signal { false };
		Type const                         _type;
		Exception_state                    _exception_state          { NO_EXCEPTION };

		Genode::Constructible<Tlb_invalidation>   _tlb_invalidation {};
		Genode::Constructible<Destroy>            _destroy {};
		Genode::Constructible<Flush_and_stop_cpu> _stop_cpu {};

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
		 * Suspend unrecoverably from execution
		 */
		void _die();

		/**
		 * In case of fault, signal to pager, and help or block
		 */
		void _signal_to_pager();

		/**
		 * Handle an exception thrown by the memory management unit
		 */
		void _mmu_exception();

		/**
		 * Handle a non-mmu exception
		 */
		void _exception();

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
		void _call_restart_thread();
		void _call_yield_thread();
		void _call_delete_thread();
		void _call_delete_pd();
		void _call_await_request_msg();
		void _call_send_request_msg();
		void _call_send_reply_msg();
		void _call_invalidate_tlb();
		void _call_cache_coherent_region();
		void _call_cache_clean_invalidate_data_region();
		void _call_cache_invalidate_data_region();
		void _call_cache_line_size();
		void _call_print_char();
		void _call_await_signal();
		void _call_pending_signal();
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
		void _call_irq_mode();
		void _call_ack_irq();
		void _call_new_obj();
		void _call_delete_obj();
		void _call_ack_cap();
		void _call_delete_cap();
		void _call_timeout();
		void _call_timeout_max_us();
		void _call_time();
		void _call_suspend();
		void _call_get_cpu_state();
		void _call_set_cpu_state();
		void _call_exception_state();
		void _call_single_step();
		void _call_ack_pager_signal();

		template <typename T>
		void _call_new(auto &&... args)
		{
			Core::Kernel_object<T> & kobj = *(Core::Kernel_object<T>*)user_arg_1();
			kobj.construct(_core_pd, args...);
			user_arg_0(kobj->core_capid());
		}

		template <typename T>
		void _call_delete()
		{
			Core::Kernel_object<T> & kobj = *(Core::Kernel_object<T>*)user_arg_1();
			kobj.destruct();
		}

		enum Ipc_alloc_result { OK, EXHAUSTED };

		[[nodiscard]] Ipc_alloc_result _ipc_alloc_recv_caps(unsigned rcv_cap_count);

		void _ipc_free_recv_caps();

		[[nodiscard]] Ipc_alloc_result _ipc_init(Genode::Native_utcb &utcb, Thread &callee);

	public:

		Genode::Align_at<Core::Cpu::Context> regs;

		/**
		 * Constructor
		 *
		 * \param priority  scheduling priority
		 * \param quota     CPU-time quota
		 * \param label     debugging label
		 * \param core      whether it is a core thread or not
		 */
		Thread(Board::Address_space_id_allocator &addr_space_id_alloc,
		       Irq::Pool                         &user_irq_pool,
		       Cpu_pool                          &cpu_pool,
		       Cpu                               &cpu,
		       Pd                                &core_pd,
		       unsigned                    const  priority,
		       unsigned                    const  quota,
		       char                 const *const  label,
		       Type                        const  type);

		/**
		 * Constructor for core/kernel thread
		 *
		 * \param label  debugging label
		 */
		Thread(Board::Address_space_id_allocator &addr_space_id_alloc,
		       Irq::Pool                         &user_irq_pool,
		       Cpu_pool                          &cpu_pool,
		       Cpu                               &cpu,
		       Pd                                &core_pd,
		       char                 const *const  label)
		:
			Thread(addr_space_id_alloc, user_irq_pool, cpu_pool, cpu,
			       core_pd, Scheduler::Priority::min(), 0, label, CORE)
		{ }

		~Thread();


		/**************************
		 ** Support for syscalls **
		 **************************/

		void user_ret_time(Kernel::time_t const t);

		void user_arg_0(Kernel::Call_arg const arg);
		void user_arg_1(Kernel::Call_arg const arg);
		void user_arg_2(Kernel::Call_arg const arg);
		void user_arg_3(Kernel::Call_arg const arg);
		void user_arg_4(Kernel::Call_arg const arg);
		void user_arg_5(Kernel::Call_arg const arg);

		Kernel::Call_arg user_arg_0() const;
		Kernel::Call_arg user_arg_1() const;
		Kernel::Call_arg user_arg_2() const;
		Kernel::Call_arg user_arg_3() const;
		Kernel::Call_arg user_arg_4() const;
		Kernel::Call_arg user_arg_5() const;

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
		static capid_t syscall_create(Core::Kernel_object<Thread> &t,
		                              unsigned const               cpu_id,
		                              unsigned const               priority,
		                              size_t const                 quota,
		                              char const * const           label)
		{
			return (capid_t)call(call_id_new_thread(), (Call_arg)&t,
			                     (Call_arg)cpu_id, (Call_arg)priority,
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
		static capid_t syscall_create(Core::Kernel_object<Thread> &t,
		                              unsigned const               cpu_id,
		                              char const * const           label)
		{
			return (capid_t)call(call_id_new_core_thread(), (Call_arg)&t,
			                     (Call_arg)cpu_id, (Call_arg)label);
		}

		/**
		 * Syscall to destroy a thread
		 *
		 * \param thread  pointer to thread kernel object
		 */
		static void syscall_destroy(Core::Kernel_object<Thread> &t) {
			call(call_id_delete_thread(), (Call_arg)&t); }


		void print(Genode::Output &out) const;


		/**************
		 ** Ipc_node **
		 **************/

		void ipc_send_request_succeeded() ;
		void ipc_send_request_failed()    ;
		void ipc_await_request_succeeded();
		void ipc_await_request_failed()   ;
		void ipc_copy_msg(Thread &sender) ;


		/*************
		 ** Signals **
		 *************/

		void signal_context_kill_pending();
		void signal_context_kill_failed();
		void signal_context_kill_done();
		void signal_wait_for_signal();
		void signal_receive_signal(void * const base, size_t const size);


		/*****************
		 ** Cpu_context **
		 *****************/

		void exception() override;
		void proceed() override;


		/*************
		 ** Timeout **
		 *************/

		void timeout_triggered() override;


		/***************
		 ** Accessors **
		 ***************/

		Object &kernel_object() { return *this; }
		char const * label() const { return _label; }
		Thread_fault fault() const { return _fault; }
		Genode::Native_utcb *utcb() { return _utcb; }
		Type type() const { return _type; }
		Exception_state exception_state() const { return _exception_state; }

		Pd &pd() const
		{
			if (_pd)
				return *_pd;

			ASSERT_NEVER_CALLED;
		}
};


/**
 * The first core thread in the system bootstrapped by the Kernel
 */
class Kernel::Core_main_thread : public Core_object<Kernel::Thread>
{
	private:

		Native_utcb _utcb_instance alignas(Hw::get_page_size()) { };

	public:

		Core_main_thread(Board::Address_space_id_allocator &addr_space_id_alloc,
		                 Irq::Pool                         &user_irq_pool,
		                 Cpu_pool                          &cpu_pool,
		                 Pd                                &core_pd);
};

#endif /* _CORE__KERNEL__THREAD_H_ */
