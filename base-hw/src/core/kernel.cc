/*
 * \brief  Singlethreaded minimalistic kernel
 * \author Martin Stein
 * \date   2011-10-20
 *
 * This kernel is the only code except the mode transition PIC, that runs in
 * privileged CPU mode. It has two tasks. First it initializes the process
 * 'core', enriches it with the whole identically mapped address range,
 * joins and applies it, assigns one thread to it with a userdefined
 * entrypoint (the core main thread) and starts this thread in userland.
 * Afterwards it is called each time an exception occurs in userland to do
 * a minimum of appropriate exception handling. Thus it holds a CPU context
 * for itself as for any other thread. But due to the fact that it never
 * relies on prior kernel runs this context only holds some constant pointers
 * such as SP and IP.
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <cpu/cpu_state.h>
#include <base/thread_state.h>

/* core includes */
#include <kernel/pd.h>
#include <platform_pd.h>
#include <trustzone.h>
#include <timer.h>

/* base-hw includes */
#include <singleton.h>

using namespace Kernel;

/* get core configuration */
extern Genode::Native_utcb * _main_utcb;
extern int _kernel_stack_high;
extern "C" void CORE_MAIN();

namespace Kernel
{
	/* import Genode types */
	typedef Genode::Thread_state Thread_state;
	typedef Genode::umword_t     umword_t;
	typedef Genode::Core_tlb     Core_tlb;
}

namespace Kernel
{
	/**
	 * Idle thread entry
	 */
	static void idle_main() { while (1) ; }


	/**
	 * Access to static kernel timer
	 */
	static Timer * timer() { static Timer _object; return &_object; }


	static void reset_lap_time()
	{
		timer()->start_one_shot(timer()->ms_to_tics(USER_LAP_TIME_MS));
	}


	/**
	 * Static kernel PD that describes core
	 */
	static Pd * core()
	{
		constexpr int tlb_align = 1 << Core_tlb::ALIGNM_LOG2;

		Core_tlb *core_tlb = unsynchronized_singleton<Core_tlb, tlb_align>();
		Pd       *pd       = unsynchronized_singleton<Pd>(core_tlb, nullptr);
		return pd;
	}

	/**
	 * Get core attributes
	 */
	unsigned core_id() { return core()->id(); }
}


namespace Kernel
{
	class Vm : public Object<Vm, MAX_VMS>,
	           public Execution_context
	{
		private:

			Genode::Cpu_state_modes * const _state;
			Signal_context * const          _context;

		public:

			void * operator new (size_t, void * p) { return p; }

			/**
			 * Constructor
			 */
			Vm(Genode::Cpu_state_modes * const state,
			   Signal_context * const context)
			: _state(state), _context(context) { }


			/**************************
			 ** Vm_session interface **
			 **************************/

			void run()   { cpu_scheduler()->insert(this); }
			void pause() { cpu_scheduler()->remove(this); }


			/***********************
			 ** Execution_context **
			 ***********************/

			void handle_exception()
			{
				switch(_state->cpu_exception) {
				case Genode::Cpu_state::INTERRUPT_REQUEST:
				case Genode::Cpu_state::FAST_INTERRUPT_REQUEST:
					handle_interrupt();
					return;
				default:
					cpu_scheduler()->remove(this);
					_context->submit(1);
				}
			}

			void proceed() { mtc()->continue_vm(_state); }
	};


	/**
	 * Access to static CPU scheduler
	 */
	Cpu_scheduler * cpu_scheduler()
	{
		/* create idle thread */
		static char idle_stack[DEFAULT_STACK_SIZE]
		__attribute__((aligned(Cpu::DATA_ACCESS_ALIGNM)));
		static Thread idle((Platform_thread *)0);
		static bool initial = 1;
		if (initial)
		{
			/* initialize idle thread */
			enum { STACK_SIZE = sizeof(idle_stack)/sizeof(idle_stack[0]) };
			void * const ip = (void *)&idle_main;
			void * const sp = (void *)&idle_stack[STACK_SIZE];

			/*
			 * Idle doesn't use its UTCB pointer, thus
			 * utcb_phys = utcb_virt = 0 is save.
			 * Base-hw doesn't support multiple cores, thus
			 * cpu_no = 0 is ok. We don't use 'start' to avoid
			 * recursive call of'cpu_scheduler()'.
			 */
			idle.prepare_to_start(ip, sp, 0, core_id(), 0, 0, 0);
			initial = 0;
		}
		/* create scheduler with a permanent idle thread */
		static Cpu_scheduler cpu_sched(&idle);
		return &cpu_sched;
	}

	/**
	 * Get attributes of the mode transition region in every PD
	 */
	addr_t mode_transition_virt_base() { return mtc()->VIRT_BASE; }
	size_t mode_transition_size()      { return mtc()->SIZE; }

	/**
	 * Get attributes of the kernel objects
	 */
	size_t thread_size()          { return sizeof(Thread); }
	size_t pd_size()              { return sizeof(Tlb) + sizeof(Pd); }
	size_t signal_context_size()  { return sizeof(Signal_context); }
	size_t signal_receiver_size() { return sizeof(Signal_receiver); }
	unsigned pd_alignm_log2()     { return Tlb::ALIGNM_LOG2; }
	size_t vm_size()              { return sizeof(Vm); }


	/**
	 * Handle the occurence of an unknown exception
	 */
	void handle_invalid_excpt() { assert(0); }


	/**
	 * Handle an interrupt request
	 */
	void handle_interrupt()
	{
		/* determine handling for specific interrupt */
		unsigned irq;
		if (pic()->take_request(irq))
		{
			switch (irq) {

			case Timer::IRQ: {

				cpu_scheduler()->yield();
				timer()->clear_interrupt();
				reset_lap_time();
				break; }

			default: {

				Irq_receiver * const o = Irq_receiver::receiver(irq);
				assert(o);
				o->receive_irq(irq);
				break; }
			}
		}
		/* disengage interrupt controller from IRQ */
		pic()->finish_request();
	}


	/**
	 * Handle request of an unknown signal type
	 */
	void handle_invalid_syscall(Thread * const) { assert(0); }


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_new_pd(Thread * const user)
	{
		/* check permissions */
		assert(user->pd_id() == core_id());

		/* create TLB and PD */
		void * dst = (void *)user->user_arg_1();
		Tlb * const tlb = new (dst) Tlb();
		dst = (void *)((addr_t)dst + sizeof(Tlb));
		Pd * const pd = new (dst) Pd(tlb, (Platform_pd *)user->user_arg_2());

		/* return success */
		user->user_arg_0(pd->id());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_new_thread(Thread * const user)
	{
		/* check permissions */
		assert(user->pd_id() == core_id());

		/* dispatch arguments */
		Syscall_arg const arg1 = user->user_arg_1();
		Syscall_arg const arg2 = user->user_arg_2();

		/* create thread */
		Thread * const t = new ((void *)arg1)
		Thread((Platform_thread *)arg2);

		/* return thread ID */
		user->user_arg_0((Syscall_ret)t->id());
	}

	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_delete_thread(Thread * const user)
	{
		/* check permissions */
		assert(user->pd_id() == core_id());

		/* get targeted thread */
		unsigned thread_id = (unsigned)user->user_arg_1();
		Thread * const thread = Thread::pool()->object(thread_id);
		assert(thread);

		/* destroy thread */
		thread->~Thread();
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_start_thread(Thread * const user)
	{
		/* check permissions */
		assert(user->pd_id() == core_id());

		/* dispatch arguments */
		Platform_thread * pt = (Platform_thread *)user->user_arg_1();
		void * const ip = (void *)user->user_arg_2();
		void * const sp = (void *)user->user_arg_3();
		unsigned const cpu_id = (unsigned)user->user_arg_4();

		/* get targeted thread */
		Thread * const t = Thread::pool()->object(pt->id());
		assert(t);

		/* start thread */
		unsigned const pd_id = pt->pd_id();
		Native_utcb * const utcb_p = pt->phys_utcb();
		Native_utcb * const utcb_v = pt->virt_utcb();
		t->start(ip, sp, cpu_id, pd_id, utcb_p, utcb_v, pt->main_thread());

		/* return software TLB that the thread is assigned to */
		Pd::Pool * const pp = Pd::pool();
		Pd * const pd = pp->object(t->pd_id());
		assert(pd);
		user->user_arg_0((Syscall_ret)pd->tlb());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_pause_thread(Thread * const user)
	{
		unsigned const tid = user->user_arg_1();

		/* shortcut for a thread to pause itself */
		if (!tid) {
			user->pause();
			user->user_arg_0(0);
			return;
		}

		/* get targeted thread and check permissions */
		Thread * const t = Thread::pool()->object(tid);
		assert(t && (user->pd_id() == core_id() || user==t));

		/* pause targeted thread */
		t->pause();
		user->user_arg_0(0);
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_resume_thread(Thread * const user)
	{
		/* get targeted thread */
		Thread * const t = Thread::pool()->object(user->user_arg_1());
		assert(t);

		/* check permissions */
		assert(user->pd_id() == core_id() || user->pd_id() == t->pd_id());

		/* resume targeted thread */
		user->user_arg_0(t->resume());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_resume_faulter(Thread * const user)
	{
		/* get targeted thread */
		Thread * const t = Thread::pool()->object(user->user_arg_1());
		assert(t);

		/* check permissions */
		assert(user->pd_id() == core_id() || user->pd_id() == t->pd_id());

		/*
		 * Writeback the TLB entry that resolves the fault.
		 * This is a substitution for write-through-flagging
		 * the memory that holds the TLB data, because the latter
		 * is not feasible in core space.
		 */
		Cpu::tlb_insertions();

		/* resume targeted thread */
		t->resume();
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_yield_thread(Thread * const user)
	{
		/* get targeted thread */
		Thread * const t = Thread::pool()->object(user->user_arg_1());

		/* invoke kernel object */
		if (t) t->resume();
		cpu_scheduler()->yield();
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_current_thread_id(Thread * const user)
	{ user->user_arg_0((Syscall_ret)user->id()); }


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_get_thread(Thread * const user)
	{
		/* check permissions */
		assert(user->pd_id() == core_id());

		/* get target */
		unsigned const tid = (unsigned)user->user_arg_1();
		Thread * t;

		/* user targets a thread by ID */
		if (tid) {
			t = Thread::pool()->object(tid);
			assert(t);

		/* user targets itself */
		} else t = user;

		/* return target platform thread */
		user->user_arg_0((Syscall_ret)t->platform_thread());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_wait_for_request(Thread * const user)
	{
		user->wait_for_request();
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_request_and_wait(Thread * const user)
	{
		/* get IPC receiver */
		Thread * const t = Thread::pool()->object(user->user_arg_1());
		assert(t);

		/* do IPC */
		user->request_and_wait(t, (size_t)user->user_arg_2());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_reply(Thread * const user) {
		user->reply((size_t)user->user_arg_1(), (bool)user->user_arg_2()); }


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_set_pager(Thread * const user)
	{
		/* check permissions */
		if (user->pd_id() != core_id()) {
			PERR("not entitled to set pager");
			return;
		}
		/* lookup faulter and pager thread */
		unsigned const pager_id = user->user_arg_1();
		Thread * const pager    = Thread::pool()->object(pager_id);
		Thread * const faulter  = Thread::pool()->object(user->user_arg_2());
		if ((pager_id && !pager) || !faulter) {
			PERR("failed to set pager");
			return;
		}
		/* assign pager */
		faulter->pager(pager);
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_update_pd(Thread * const user)
	{
		assert(user->pd_id() == core_id());
		Cpu::flush_tlb_by_pid(user->user_arg_1());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_update_region(Thread * const user)
	{
		assert(user->pd_id() == core_id());

		/* FIXME we don't handle instruction caches by now */
		Cpu::flush_data_cache_by_virt_region((addr_t)user->user_arg_1(),
		                                     (size_t)user->user_arg_2());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_allocate_irq(Thread * const user)
	{
		assert(user->pd_id() == core_id());
		unsigned irq = user->user_arg_1();
		user->user_arg_0(user->allocate_irq(irq));
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_free_irq(Thread * const user)
	{
		assert(user->pd_id() == core_id());
		unsigned irq = user->user_arg_1();
		user->user_arg_0(user->free_irq(irq));
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_await_irq(Thread * const user)
	{
		assert(user->pd_id() == core_id());
		user->await_irq();
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_print_char(Thread * const user)
	{
		Genode::printf("%c", (char)user->user_arg_1());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_read_thread_state(Thread * const user)
	{
		assert(user->pd_id() == core_id());
		Thread * const t = Thread::pool()->object(user->user_arg_1());
		if (!t) PDBG("Targeted thread unknown");
		Thread_state * const ts = (Thread_state *)user->phys_utcb()->base();
		t->Cpu::Context::read_cpu_state(ts);
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_write_thread_state(Thread * const user)
	{
		assert(user->pd_id() == core_id());
		Thread * const t = Thread::pool()->object(user->user_arg_1());
		if (!t) PDBG("Targeted thread unknown");
		Thread_state * const ts = (Thread_state *)user->phys_utcb()->base();
		t->Cpu::Context::write_cpu_state(ts);
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_new_signal_receiver(Thread * const user)
	{
		/* check permissions */
		if (user->pd_id() != core_id()) {
			PERR("not entitled to create signal receiver");
			user->user_arg_0(0);
			return;
		}
		/* create receiver */
		void * p = (void *)user->user_arg_1();
		Signal_receiver * const r = new (p) Signal_receiver();
		user->user_arg_0(r->id());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_new_signal_context(Thread * const user)
	{
		/* check permissions */
		if (user->pd_id() != core_id()) {
			PERR("not entitled to create signal context");
			user->user_arg_0(0);
			return;
		}
		/* lookup receiver */
		unsigned id = user->user_arg_2();
		Signal_receiver * const r = Signal_receiver::pool()->object(id);
		if (!r) {
			PERR("unknown signal receiver");
			user->user_arg_0(0);
			return;
		}
		/* create and assign context*/
		void * p = (void *)user->user_arg_1();
		unsigned imprint = user->user_arg_3();
		if (r->new_context(p, imprint)) {
			PERR("failed to create signal context");
			user->user_arg_0(0);
			return;
		}
		/* return context name */
		Signal_context * const c = (Signal_context *)p;
		user->user_arg_0(c->id());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_await_signal(Thread * const user)
	{
		/* lookup receiver */
		unsigned id = user->user_arg_1();
		Signal_receiver * const r = Signal_receiver::pool()->object(id);
		if (!r) {
			PERR("unknown signal receiver");
			user->user_arg_0(-1);
			return;
		}
		/* register handler at the receiver */
		if (r->add_handler(user)) {
			PERR("failed to register handler at signal receiver");
			user->user_arg_0(-1);
			return;
		}
		user->user_arg_0(0);
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_signal_pending(Thread * const user)
	{
		/* lookup signal receiver */
		unsigned id = user->user_arg_1();
		Signal_receiver * const r = Signal_receiver::pool()->object(id);
		if (!r) {
			PERR("unknown signal receiver");
			user->user_arg_0(0);
			return;
		}
		/* get pending state */
		user->user_arg_0(r->deliverable());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_submit_signal(Thread * const user)
	{
		/* lookup signal context */
		unsigned const id = user->user_arg_1();
		Signal_context * const c = Signal_context::pool()->object(id);
		if(!c) {
			PERR("unknown signal context");
			user->user_arg_0(-1);
			return;
		}
		/* trigger signal context */
		if (c->submit(user->user_arg_2())) {
			PERR("failed to submit signal context");
			user->user_arg_0(-1);
			return;
		}
		user->user_arg_0(0);
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_ack_signal(Thread * const user)
	{
		/* lookup signal context */
		unsigned id = user->user_arg_1();
		Signal_context * const c = Signal_context::pool()->object(id);
		if (!c) {
			PERR("unknown signal context");
			return;
		}
		/* acknowledge */
		c->ack();
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_kill_signal_context(Thread * const user)
	{
		/* check permissions */
		if (user->pd_id() != core_id()) {
			PERR("not entitled to kill signal context");
			user->user_arg_0(-1);
			return;
		}
		/* lookup signal context */
		unsigned id = user->user_arg_1();
		Signal_context * const c = Signal_context::pool()->object(id);
		if (!c) {
			PERR("unknown signal context");
			user->user_arg_0(0);
			return;
		}
		/* kill signal context */
		if (c->kill(user)) {
			PERR("failed to kill signal context");
			user->user_arg_0(-1);
			return;
		}
		user->user_arg_0(0);
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_kill_signal_receiver(Thread * const user)
	{
		/* check permissions */
		if (user->pd_id() != core_id()) {
			PERR("not entitled to kill signal receiver");
			user->user_arg_0(-1);
			return;
		}
		/* lookup signal receiver */
		unsigned id = user->user_arg_1();
		Signal_receiver * const r = Signal_receiver::pool()->object(id);
		if (!r) {
			PERR("unknown signal receiver");
			user->user_arg_0(0);
			return;
		}
		/* kill signal receiver */
		if (r->kill(user)) {
			PERR("unknown signal receiver");
			user->user_arg_0(-1);
			return;
		}
		user->user_arg_0(0);
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_new_vm(Thread * const user)
	{
		/* check permissions */
		assert(user->pd_id() == core_id());

		/* dispatch arguments */
		void * const allocator = (void * const)user->user_arg_1();
		Genode::Cpu_state_modes * const state =
			(Genode::Cpu_state_modes * const)user->user_arg_2();
		Signal_context * const context =
			Signal_context::pool()->object(user->user_arg_3());
		assert(context);

		/* create vm */
		Vm * const vm = new (allocator) Vm(state, context);

		/* return vm id */
		user->user_arg_0((Syscall_ret)vm->id());
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_run_vm(Thread * const user)
	{
		/* check permissions */
		assert(user->pd_id() == core_id());

		/* get targeted vm via its id */
		Vm * const vm = Vm::pool()->object(user->user_arg_1());
		assert(vm);

		/* run targeted vm */
		vm->run();
	}


	/**
	 * Do specific syscall for 'user', for details see 'syscall.h'
	 */
	void do_pause_vm(Thread * const user)
	{
		/* check permissions */
		assert(user->pd_id() == core_id());

		/* get targeted vm via its id */
		Vm * const vm = Vm::pool()->object(user->user_arg_1());
		assert(vm);

		/* pause targeted vm */
		vm->pause();
	}


	/**
	 * Handle a syscall request
	 *
	 * \param user  thread that called the syscall
	 */
	void handle_syscall(Thread * const user)
	{
		switch (user->user_arg_0())
		{
		case NEW_THREAD:           do_new_thread(user); return;
		case DELETE_THREAD:        do_delete_thread(user); return;
		case START_THREAD:         do_start_thread(user); return;
		case PAUSE_THREAD:         do_pause_thread(user); return;
		case RESUME_THREAD:        do_resume_thread(user); return;
		case RESUME_FAULTER:       do_resume_faulter(user); return;
		case GET_THREAD:           do_get_thread(user); return;
		case CURRENT_THREAD_ID:    do_current_thread_id(user); return;
		case YIELD_THREAD:         do_yield_thread(user); return;
		case READ_THREAD_STATE:    do_read_thread_state(user); return;
		case WRITE_THREAD_STATE:   do_write_thread_state(user); return;
		case REQUEST_AND_WAIT:     do_request_and_wait(user); return;
		case REPLY:                do_reply(user); return;
		case WAIT_FOR_REQUEST:     do_wait_for_request(user); return;
		case SET_PAGER:            do_set_pager(user); return;
		case UPDATE_PD:            do_update_pd(user); return;
		case UPDATE_REGION:        do_update_region(user); return;
		case NEW_PD:               do_new_pd(user); return;
		case ALLOCATE_IRQ:         do_allocate_irq(user); return;
		case AWAIT_IRQ:            do_await_irq(user); return;
		case FREE_IRQ:             do_free_irq(user); return;
		case PRINT_CHAR:           do_print_char(user); return;
		case NEW_SIGNAL_RECEIVER:  do_new_signal_receiver(user); return;
		case NEW_SIGNAL_CONTEXT:   do_new_signal_context(user); return;
		case KILL_SIGNAL_CONTEXT:  do_kill_signal_context(user); return;
		case KILL_SIGNAL_RECEIVER: do_kill_signal_receiver(user); return;
		case AWAIT_SIGNAL:         do_await_signal(user); return;
		case SUBMIT_SIGNAL:        do_submit_signal(user); return;
		case SIGNAL_PENDING:       do_signal_pending(user); return;
		case ACK_SIGNAL:           do_ack_signal(user); return;
		case NEW_VM:               do_new_vm(user); return;
		case RUN_VM:               do_run_vm(user); return;
		case PAUSE_VM:             do_pause_vm(user); return;
		default:
			PERR("invalid syscall");
			user->crash();
			reset_lap_time();
		}
	}
}

/**
 * Prepare the system for the first run of 'kernel'
 */
extern "C" void init_phys_kernel() {
	Cpu::init_phys_kernel(); }

/**
 * Kernel main routine
 */
extern "C" void kernel()
{
	static bool initial_call = true;

	/* an exception occured */
	if (!initial_call)
	{
		/* handle exception that interrupted the last user */
		cpu_scheduler()->head()->handle_exception();

	/* kernel initialization */
	} else {

		Genode::printf("Kernel started!\n");

		/* enable kernel timer */
		pic()->unmask(Timer::IRQ);

		/* TrustZone initialization code */
		trustzone_initialization(pic());

		/* switch to core address space */
		Cpu::init_virt_kernel(core()->tlb()->base(), core_id());

		/*
		 * From this point on, it is safe to use 'cmpxchg', i.e., to create
		 * singleton objects via the static-local object pattern. See
		 * the comment in 'src/base/singleton.h'.
		 */

		/* create the core main thread */
		static Native_utcb cm_utcb;
		static char cm_stack[DEFAULT_STACK_SIZE]
		            __attribute__((aligned(Cpu::DATA_ACCESS_ALIGNM)));
		static Thread core_main((Platform_thread *)0);
		_main_utcb = &cm_utcb;
		enum { CM_STACK_SIZE = sizeof(cm_stack)/sizeof(cm_stack[0]) + 1 };
		core_main.start((void *)CORE_MAIN,
		                (void *)&cm_stack[CM_STACK_SIZE - 1],
		                0, core_id(), &cm_utcb, &cm_utcb, 1);

		/* kernel initialization finished */
		reset_lap_time();
		initial_call = false;
	}
	/* will jump to the context related mode-switch */
	cpu_scheduler()->head()->proceed();
}

static Kernel::Mode_transition_control * Kernel::mtc()
{
	/* compose CPU context for kernel entry */
	struct Kernel_context : Cpu::Context
	{
		/**
		 * Constructor
		 */
		Kernel_context()
		{
			ip = (addr_t)kernel;
			sp = (addr_t)&_kernel_stack_high;
			core()->admit(this);
		}
	} * const k = unsynchronized_singleton<Kernel_context>();

	/* initialize mode transition page */
	return unsynchronized_singleton<Mode_transition_control>(k);
}
