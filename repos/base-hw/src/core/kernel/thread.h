/*
 * \brief   Kernel backend for execution contexts in userland
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2025 Genode Labs GmbH
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
#include <kernel/vcpu.h>
#include <object.h>
#include <kernel/interface.h>
#include <assertion.h>

#include <hw/util.h>

/* base internal includes */
#include <base/internal/native_utcb.h>

namespace Kernel {

	class Cpu_pool;
	struct Thread_fault;
	class Thread;
	class Core_thread;
	class Idle_thread;
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


class Kernel::Thread : private Kernel::Object, public Cpu_context, private Timeout
{
	public:

		enum Exception_state { NO_EXCEPTION, MMU_FAULT, EXCEPTION };

	protected:

		friend class Core_thread;

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

		void *_obj_id_ref_ptr[MAX_RCV_CAPS] { nullptr };
		Ipc_node _ipc_node;
		capid_t  _ipc_capid                { cap_id_invalid() };
		size_t   _ipc_rcv_caps             { 0 };

		Genode::Native_utcb *_utcb { nullptr };

		Pd &_pd;

		struct Fault_context
		{
			Thread         &pager;
			Signal_context &sc;
		};

		Genode::Constructible<Fault_context> _fault_context {};

		Thread_fault           _fault                    { };
		State                  _state;
		Signal_handler         _signal_handler           { *this };
		Signal_context_killer  _signal_context_killer    { *this };
		char   const *const    _label;
		capid_t                _timeout_sigid            { 0 };
		bool                   _paused                   { false };
		Exception_state        _exception_state          { NO_EXCEPTION };

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
		void _die()
		{
			Genode::error("Will halt thread: ", *this);
			_become_inactive(DEAD);
		}

		void _die(auto &&... args)
		{
			Genode::error(args...);
			_die();
		}

		/**
		 * In case of fault, signal to pager, and help or block
		 */
		void _signal_to_pager();

		/**
		 * Handle an exception thrown by the memory management unit
		 */
		virtual void _mmu_exception();

		/**
		 * Handle a non-mmu exception
		 */
		virtual void _exception();

		/**
		 * Handle kernel-call request of the thread
		 */
		virtual void _call();

		void _pause();
		bool _restart();

		enum Ipc_alloc_result { OK, EXHAUSTED };

		[[nodiscard]] Ipc_alloc_result _ipc_alloc_recv_caps(unsigned rcv_cap_count);

		void _ipc_free_recv_caps();

		[[nodiscard]] Ipc_alloc_result _ipc_init(Genode::Native_utcb &utcb, Thread &callee);

		virtual void _save(Genode::Cpu_state &);

		virtual bool _privileged() const { return false; }


		/*********************************************************
		 ** Kernel-call back-ends, see kernel-interface headers **
		 *********************************************************/

		virtual Thread_restart_result _call_thread_restart(capid_t const);

		void _call_thread_stop();

		Rpc_result _call_rpc_wait(unsigned);
		Rpc_result _call_rpc_call(capid_t const, unsigned);
		Rpc_result _call_rpc_reply_and_wait(unsigned);
		void       _call_rpc_reply();

		void   _call_cache_coherent(addr_t const, size_t const);
		void   _call_cache_clean_invalidate(addr_t const, size_t const);
		void   _call_cache_invalidate(addr_t const, size_t const);
		size_t _call_cache_line_size();

		Signal_result _call_signal_wait(capid_t const);
		Signal_result _call_signal_pending(capid_t const);
		void          _call_signal_submit(capid_t const, unsigned);
		void          _call_signal_ack(capid_t const);
		void          _call_signal_kill(capid_t const);

		void _call_cap_ack(capid_t const);
		void _call_cap_destroy(capid_t const);

		void _call_timeout(timeout_t const, capid_t const);

		void _call_vcpu_pause(capid_t const);
		void _call_vcpu_run(capid_t const);

		/**
		 * Constructor for idle thread
		 */
		Thread(Cpu &cpu, Pd &pd);

		/**
		 * Constructor for core threads
		 */
		Thread(Cpu &cpu, Pd &pd, char const *const label);

	public:

		Genode::Align_at<Board::Cpu::Context> regs;

		/*
		 * Noncopyable
		 */
		Thread(Thread const &) = delete;
		Thread &operator = (Thread const &) = delete;

		/**
		 * Constructor for non-privileged threads
		 */
		Thread(Cpu &cpu, Pd &pd, Scheduler::Group_id const group_id,
		       char const *const label);

		virtual ~Thread();


		/**************************
		 ** Support for syscalls **
		 **************************/

		void user_ret_time(Kernel::time_t const t);

		void user_ret(auto const arg) { regs->reg_0((Call_arg)arg); }

		template <typename T> T user_arg_0() const { return (T)regs->reg_0(); }
		template <typename T> T user_arg_1() const { return (T)regs->reg_1(); }
		template <typename T> T user_arg_2() const { return (T)regs->reg_2(); }
		template <typename T> T user_arg_3() const { return (T)regs->reg_3(); }
		template <typename T> T user_arg_4() const { return (T)regs->reg_4(); }
		template <typename T> T user_arg_5() const { return (T)regs->reg_5(); }

		/**
		 * Syscall to create a thread
		 *
		 * \param p         memory donation for the new kernel thread object
		 * \param group_id  scheduling group id of the new thread
		 * \param label     debugging label of the new thread
		 *
		 * \retval capability id of the new kernel object
		 */
		static capid_t syscall_create(Core::Kernel_object<Thread> &t,
		                              Pd                          &pd,
		                              unsigned const               cpu_id,
		                              unsigned const               group_id,
		                              char const * const           label)
		{
			return (capid_t)core_call(Core_call_id::THREAD_CREATE,
			                          (Call_arg)&t, (Call_arg)&pd,
			                          (Call_arg)cpu_id, (Call_arg)group_id,
			                          (Call_arg)label);
		}

		/**
		 * Syscall to destroy a thread
		 *
		 * \param thread  pointer to thread kernel object
		 */
		static void syscall_destroy(Core::Kernel_object<Thread> &t) {
			core_call(Core_call_id::THREAD_DESTROY, (Call_arg)&t); }


		void print(Genode::Output &out) const;


		/**************
		 ** Ipc_node **
		 **************/

		void ipc_send_request_succeeded() ;
		void ipc_send_request_failed()    ;
		void ipc_await_request_succeeded();
		void ipc_copy_msg(Thread &sender) ;


		/*************
		 ** Signals **
		 *************/

		void signal_context_kill_done();
		void signal_receive_signal(void * const base, size_t const size);


		/*****************
		 ** Cpu_context **
		 *****************/

		void exception(Genode::Cpu_state&) override;
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
		Exception_state exception_state() const { return _exception_state; }
};


class Kernel::Core_thread : public Kernel::Thread
{
	private:

		/**
		 * A TLB invalidation may need cross-cpu synchronization
		 */
		struct Tlb_invalidation : Inter_processor_work
		{
			Inter_processor_work_list &global_work_list;
			Core_thread               &caller; /* the caller gets blocked until all finished */
			Pd                        &pd;     /* the corresponding pd */
			addr_t                     addr;
			size_t                     size;
			unsigned                   cnt;    /* count of cpus left */

			Tlb_invalidation(Inter_processor_work_list &global_work_list,
			                 Core_thread               &caller,
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
		 * The destruction of a thread/vcpu still active on another cpu
		 * needs cross-cpu synchronization
		 */
		template <typename OBJ>
		struct Destroy : Inter_processor_work
		{
			using Obj = Core::Kernel_object<OBJ>;

			Core_thread &_caller; /* the caller gets blocked till the end */
			Obj         &_obj_to_destroy; /* obj to be destroyed */

			Destroy(Cpu &cpu, Core_thread &caller, Obj &to_destroy);


			/************************************
			 ** Inter_processor_work interface **
			 ************************************/

			void execute(Cpu &) override;
		};

		/**
		 * Object to pause a thread still active on another cpu
		 * needs cross-cpu synchronization
		 */
		struct Pause : Inter_processor_work
		{
			Core_thread &_caller; /* the caller gets blocked till the end */
			Thread      &_thread; /* threaded to be paused */

			Pause(Cpu &cpu, Core_thread &caller, Thread &thread);


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

	protected:

		Board::Address_space_id_allocator &_addr_space_id_alloc;

		Cpu_pool &_cpu_pool;

		Genode::Constructible<Tlb_invalidation>   _tlb_invalidation {};
		Genode::Constructible<Destroy<Thread>>    _thread_destroy {};
		Genode::Constructible<Destroy<Vcpu>>      _vcpu_destroy {};
		Genode::Constructible<Flush_and_stop_cpu> _stop_cpu {};
		Genode::Constructible<Pause>              _thread_pause {};

		virtual void _mmu_exception() override;
		virtual void _exception() override;

		virtual bool _privileged() const override { return true; }


		/*********************************************************
		 ** Kernel-call back-ends, see kernel-interface headers **
		 *********************************************************/

		void _call() override;

		using C_thread = Core::Kernel_object<Thread>;
		using C_pd     = Core::Kernel_object<Pd>;
		using C_irq    = Core::Kernel_object<User_irq>;
		using C_vcpu   = Core::Kernel_object<Vcpu>;

		using Thread_identity =
			Genode::Constructible<Core_object_identity<Thread>>;

		Rpc_result _call_thread_start(Thread&, Native_utcb&);
		void _call_thread_pause(Thread &);
		void _call_thread_resume(Thread &);
		void _call_thread_destroy(C_thread &);
		void _call_thread_pager(Thread &, Thread &, capid_t);
		void _call_thread_pager_signal_ack(capid_t, Thread &, bool);

		Thread_restart_result _call_thread_restart(capid_t const) override;

		void _call_pd_invalidate_tlb(Pd &, addr_t const, size_t const);
		void _call_pd_destroy(C_pd &);

		capid_t _call_vcpu_create(C_vcpu &, Call_arg, Board::Vcpu_state &,
		                          Vcpu::Identity &, capid_t sig_cap);
		void _call_vcpu_destroy(C_vcpu &);

		capid_t _call_irq_create(C_irq &, unsigned,
		                         Genode::Irq_session::Trigger,
                                 Genode::Irq_session::Polarity, capid_t id);

		capid_t _call_obj_create(Thread_identity &, capid_t);

		Cpu_suspend_result _call_cpu_suspend(unsigned const);

		template <typename T>
		void _call_create(auto &&... args)
		{
			Core::Kernel_object<T> &kobj =
				*user_arg_1<Core::Kernel_object<T>*>();
			kobj.construct(_pd, args...);
			user_ret(kobj->core_capid());
		}

		template <typename T>
		void _call_destruct()
		{
			Core::Kernel_object<T> &kobj =
				*user_arg_1<Core::Kernel_object<T>*>();
			kobj.destruct();
		}

	public:

		/*
		 * Noncopyable
		 */
		Core_thread(Core_thread const &) = delete;
		Core_thread &operator = (Core_thread const &) = delete;

		Core_thread(Board::Address_space_id_allocator &addr_space_id_alloc,
		            Cpu_pool                          &cpu_pool,
		            Cpu                               &cpu,
		            Pd                                &core_pd,
		            char                  const *const label)
		:
			Thread(cpu, core_pd, label),
			_addr_space_id_alloc(addr_space_id_alloc),
			_cpu_pool(cpu_pool)
		{ }


		/**
		 * Syscall to create a core thread
		 *
		 * \param p         memory donation for the new kernel thread object
		 * \param label     debugging label of the new thread
		 *
		 * \retval capability id of the new kernel object
		 */
		static capid_t syscall_create(Core::Kernel_object<Core_thread> &t,
		                              unsigned const cpu_id,
		                              char const * const label)
		{
			return (capid_t)core_call(Core_call_id::THREAD_CORE_CREATE,
			                          (Call_arg)&t, (Call_arg)cpu_id,
			                          (Call_arg)label);
		}

		/**
		 * Syscall to destroy a thread
		 *
		 * \param thread  pointer to thread kernel object
		 */
		static void syscall_destroy(Core::Kernel_object<Core_thread> &t) {
			core_call(Core_call_id::THREAD_DESTROY, (Call_arg)&t); }
};


class Kernel::Idle_thread : public Thread
{
	private:

		virtual bool _privileged() const override { return true; }
		virtual void _save(Genode::Cpu_state &) override {}

	public:

		Idle_thread(Cpu &cpu, Pd &core_pd);
};


/**
 * The first core thread in the system bootstrapped by the Kernel
 */
class Kernel::Core_main_thread : public Core_object<Kernel::Core_thread>
{
	private:

		Native_utcb _utcb_instance alignas(Hw::get_page_size()) { };

	public:

		Core_main_thread(Board::Address_space_id_allocator &addr_space_id_alloc,
		                 Cpu_pool                          &cpu_pool,
		                 Pd                                &core_pd);
};

#endif /* _CORE__KERNEL__THREAD_H_ */
