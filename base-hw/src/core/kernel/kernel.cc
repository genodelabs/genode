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

/* core includes */
#include <kernel/pd.h>
#include <kernel/vm.h>
#include <kernel/irq.h>
#include <platform_pd.h>
#include <trustzone.h>
#include <timer.h>
#include <pic.h>

/* base includes */
#include <unmanaged_singleton.h>

/* base-hw includes */
#include <kernel/perf_counter.h>

using namespace Kernel;

/* get core configuration */
extern Genode::Native_utcb * _main_thread_utcb;
extern int _kernel_stack_high;
extern "C" void CORE_MAIN();

namespace Kernel
{
	/**
	 * Return interrupt-controller singleton
	 */
	Pic * pic() { return unmanaged_singleton<Pic>(); }

	/* import Genode types */
	typedef Genode::umword_t       umword_t;
	typedef Genode::Core_tlb       Core_tlb;
	typedef Genode::Core_thread_id Core_thread_id;

	void init_platform();
}

namespace Kernel
{
	/**
	 * Idle thread entry
	 */
	static void idle_main() { while (1) ; }

	Pd_ids * pd_ids() { return unmanaged_singleton<Pd_ids>(); }
	Thread_ids * thread_ids() { return unmanaged_singleton<Thread_ids>(); }
	Signal_context_ids * signal_context_ids() { return unmanaged_singleton<Signal_context_ids>(); }
	Signal_receiver_ids * signal_receiver_ids() { return unmanaged_singleton<Signal_receiver_ids>(); }

	Pd_pool * pd_pool() { return unmanaged_singleton<Pd_pool>(); }
	Thread_pool * thread_pool() { return unmanaged_singleton<Thread_pool>(); }
	Signal_context_pool * signal_context_pool() { return unmanaged_singleton<Signal_context_pool>(); }
	Signal_receiver_pool * signal_receiver_pool() { return unmanaged_singleton<Signal_receiver_pool>(); }

	/**
	 * Access to static kernel timer
	 */
	static Timer * timer() { static Timer _object; return &_object; }


	void reset_lap_time()
	{
		timer()->start_one_shot(timer()->ms_to_tics(USER_LAP_TIME_MS));
	}


	/**
	 * Static kernel PD that describes core
	 */
	static Pd * core()
	{
		constexpr int tlb_align = 1 << Core_tlb::ALIGNM_LOG2;
		Core_tlb *core_tlb = unmanaged_singleton<Core_tlb, tlb_align>();
		Pd       *pd       = unmanaged_singleton<Pd>(core_tlb, nullptr);
		return pd;
	}

	/**
	 * Get core attributes
	 */
	unsigned core_id() { return core()->id(); }
}


namespace Kernel
{
	/**
	 * Access to static CPU scheduler
	 */
	Cpu_scheduler * cpu_scheduler()
	{
		/* create idle thread */
		static char idle_stack[DEFAULT_STACK_SIZE]
		__attribute__((aligned(Cpu::DATA_ACCESS_ALIGNM)));
		static Thread idle(Priority::MAX, "idle");
		static bool init = 0;
		if (!init) {
			enum { STACK_SIZE = sizeof(idle_stack)/sizeof(idle_stack[0]) };
			idle.ip = (addr_t)&idle_main;;
			idle.sp = (addr_t)&idle_stack[STACK_SIZE];;
			idle.init(0, core_id(), 0, 0);
			init = 1;
		}
		/* create CPU scheduler with a permanent idle thread */
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
	 * Handle an interrupt request
	 */
	void handle_interrupt()
	{
		/* determine handling for specific interrupt */
		unsigned irq_id;
		if (pic()->take_request(irq_id))
		{
			switch (irq_id) {

			case Timer::IRQ: {

				cpu_scheduler()->yield();
				timer()->clear_interrupt();
				reset_lap_time();
				break; }

			default: {

				Irq::occurred(irq_id);
				break; }
			}
		}
		/* disengage interrupt controller from IRQ */
		pic()->finish_request();
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

	/* an exception occurred */
	if (!initial_call)
	{
		/* handle exception that interrupted the last user */
		cpu_scheduler()->head()->handle_exception();

	/* kernel initialization */
	} else {

		/* enable kernel timer */
		pic()->unmask(Timer::IRQ);

		/* TrustZone initialization code */
		trustzone_initialization(pic());

		/* enable performance counter */
		perf_counter()->enable();

		/* switch to core address space */
		Cpu::init_virt_kernel(core()->tlb()->base(), core_id());

		/*
		 * From this point on, it is safe to use 'cmpxchg', i.e., to create
		 * singleton objects via the static-local object pattern. See
		 * the comment in 'src/base/singleton.h'.
		 */

		/* create the core main thread */
		{
			/* get stack memory that fullfills the constraints for core stacks */
			enum {
				STACK_ALIGNM = 1 << Genode::CORE_STACK_ALIGNM_LOG2,
				STACK_SIZE   = DEFAULT_STACK_SIZE,
			};
			if (STACK_SIZE > STACK_ALIGNM - sizeof(Core_thread_id)) {
				PERR("stack size does not fit stack alignment of core");
			}
			static char s[STACK_SIZE] __attribute__((aligned(STACK_ALIGNM)));

			/* provide thread ident at the aligned base of the stack */
			*(Core_thread_id *)s = 0;

			/* start thread with stack pointer at the top of stack */
			static Native_utcb utcb;
			static Thread t(Priority::MAX, "core");
			_main_thread_utcb = &utcb;
			_main_thread_utcb->start_info()->init(t.id());
			t.ip = (addr_t)CORE_MAIN;;
			t.sp = (addr_t)s + STACK_SIZE;
			t.init(0, core_id(), &utcb, 1);
		}
		/* kernel initialization finished */
		init_platform();
		reset_lap_time();
		initial_call = false;
	}
	/* will jump to the context related mode-switch */
	cpu_scheduler()->head()->proceed();
}

Kernel::Mode_transition_control * Kernel::mtc()
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
	} * const k = unmanaged_singleton<Kernel_context>();

	/* initialize mode transition page */
	return unmanaged_singleton<Mode_transition_control>(k);
}
