/*
 * \brief  Singlethreaded minimalistic kernel
 * \author Martin Stein
 * \author Stefan Kalkowski
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
#include <kernel/lock.h>
#include <kernel/pd.h>
#include <platform_pd.h>
#include <trustzone.h>
#include <timer.h>
#include <pic.h>
#include <map_local.h>

/* base includes */
#include <unmanaged_singleton.h>
#include <base/native_types.h>

/* base-hw includes */
#include <kernel/irq.h>
#include <kernel/perf_counter.h>

using namespace Kernel;

extern "C" void _core_start(void);
extern void *   _start_secondary_cpus;

static_assert(sizeof(Genode::sizet_arithm_t) >= 2 * sizeof(size_t),
	"Bad result type for size_t arithmetics.");

namespace Kernel
{
	/* import Genode types */
	typedef Genode::Core_thread_id Core_thread_id;

	Pd_pool * pd_pool() { return unmanaged_singleton<Pd_pool>(); }
	Thread_pool * thread_pool() { return unmanaged_singleton<Thread_pool>(); }
	Signal_context_pool * signal_context_pool() { return unmanaged_singleton<Signal_context_pool>(); }
	Signal_receiver_pool * signal_receiver_pool() { return unmanaged_singleton<Signal_receiver_pool>(); }

	/**
	 * Hook that enables automated testing of kernel internals
	 */
	void test();

	enum { STACK_SIZE = 64 * 1024 };

	/**
	 * Return lock that guards all kernel data against concurrent access
	 */
	Lock & data_lock()
	{
		static Lock s;
		return s;
	}
}


Kernel::Id_allocator & Kernel::id_alloc() {
	return *unmanaged_singleton<Id_allocator>(); }


Pd * Kernel::core_pd() {
	return unmanaged_singleton<Genode::Core_platform_pd>()->kernel_pd(); }


Pic * Kernel::pic() { return unmanaged_singleton<Pic>(); }


Native_utcb* Kernel::core_main_thread_utcb_phys_addr() {
	return unmanaged_singleton<Native_utcb,Genode::get_page_size()>(); }


/**
 * Enable kernel-entry assembly to get an exclusive stack for every CPU
 */
unsigned kernel_stack_size = Kernel::STACK_SIZE;
char     kernel_stack[NR_OF_CPUS][Kernel::STACK_SIZE]
         __attribute__((aligned(16)));


/**
 * Setup kernel environment before activating secondary CPUs
 */
extern "C" void init_kernel_up()
{
	/*
	 * As atomic operations are broken in physical mode on some platforms
	 * we must avoid the use of 'cmpxchg' by now (includes not using any
	 * local static objects.
	 */

	/* calculate in advance as needed later when data writes aren't allowed */
	core_pd();

	/* initialize all CPU objects */
	cpu_pool();

	/* initialize PIC */
	pic();

	/* go multiprocessor mode */
	Cpu::start_secondary_cpus(&_start_secondary_cpus);
}


/**
 * Setup kernel enviroment after activating secondary CPUs as primary CPU
 */
void init_kernel_mp_primary()
{
	using namespace Genode;

	/* get stack memory that fullfills the constraints for core stacks */
	enum {
		STACK_ALIGNM = 1 << CORE_STACK_ALIGNM_LOG2,
		STACK_SIZE   = DEFAULT_STACK_SIZE,
	};
	static_assert(STACK_SIZE <= STACK_ALIGNM - sizeof(Core_thread_id),
	              "stack size does not fit stack alignment of core");
	static char s[STACK_SIZE] __attribute__((aligned(STACK_ALIGNM)));

	/* provide thread ident at the aligned base of the stack */
	*(Core_thread_id *)s = 0;

	/* initialize UTCB and map it */
	Native_utcb * utcb = Kernel::core_main_thread_utcb_phys_addr();
	Genode::map_local((addr_t)utcb, (addr_t)utcb_main_thread(),
	                  sizeof(Native_utcb) / get_page_size());

	static Kernel::Thread t(Cpu_priority::max, 0, "core");

	/* start thread with stack pointer at the top of stack */
	utcb->start_info()->init(t.id(), Dataspace_capability());
	t.ip = (addr_t)&_core_start;
	t.sp = (addr_t)s + STACK_SIZE;
	t.init(cpu_pool()->primary_cpu(), core_pd(),
	       Genode::utcb_main_thread(), 1);

	/* kernel initialization finished */
	Genode::printf("kernel initialized\n");
	test();
}


/**
 * Setup kernel enviroment after activating secondary CPUs
 */
extern "C" void init_kernel_mp()
{
	/*
	 * As updates on a cached kernel lock might not be visible to CPUs that
	 * have not enabled caches, we can't synchronize the activation of MMU and
	 * caches. Hence we must avoid write access to kernel data by now.
	 */

	/* synchronize data view of all CPUs */
	Cpu::invalidate_data_caches();
	Cpu::invalidate_instr_caches();
	Cpu::data_synchronization_barrier();

	/* locally initialize interrupt controller */
	pic()->init_cpu_local();

	/* initialize CPU in physical mode */
	Cpu::init_phys_kernel();

	/* switch to core address space */
	Cpu::init_virt_kernel(core_pd());

	/*
	 * Now it's safe to use 'cmpxchg'
	 */

	Lock::Guard guard(data_lock());

	/*
	 * Now it's save to write to kernel data
	 */

	/*
	 * TrustZone initialization code
	 *
	 * FIXME This is a plattform specific feature
	 */
	init_trustzone(pic());

	/*
	 * Enable performance counter
	 *
	 * FIXME This is an optional CPU specific feature
	 */
	perf_counter()->enable();

	/* enable timer interrupt */
	unsigned const cpu = Cpu::executing_id();
	pic()->unmask(Timer::interrupt_id(cpu), cpu);

	/* do further initialization only as primary CPU */
	if (Cpu::primary_id() != cpu) { return; }
	init_kernel_mp_primary();
}


/**
 * Main routine of every kernel pass
 */
extern "C" void kernel()
{
	data_lock().lock();
	cpu_pool()->cpu(Cpu::executing_id())->exception();
}


Kernel::Cpu_context::Cpu_context(Genode::Translation_table * const table)
{
	_init(STACK_SIZE, (addr_t)table);
	sp = (addr_t)kernel_stack;
	ip = (addr_t)kernel;
	core_pd()->admit(this);
}
