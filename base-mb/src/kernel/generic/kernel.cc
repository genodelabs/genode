/*
 * \brief  Kernel initialization
 * \author Martin Stein
 * \date   2010-07-22
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <platform/platform.h>
#include <generic/scheduler.h>
#include "include/thread.h"
#include <generic/printf.h>
#include <generic/verbose.h>
#include <generic/blocking.h>
#include <kernel/config.h>
#include <kernel/syscalls.h>
#include <cpu/prints.h>
#include <util/array.h>

using namespace Cpu;
using namespace Kernel;

enum { KERNEL__VERBOSE = 0,
       KERNEL__WARNING = 1,
       KERNEL__ERROR   = 1 };


/* Can be defined via compiler options */
extern "C" void ROOTTASK_ENTRY();

extern Kernel::Exec_context* _userland_context;
extern Utcb* _main_utcb_addr;

extern int _exit_kernel;


Kernel::Thread_factory* Kernel::thread_factory()
{
	static Thread_factory _tf;
	return &_tf;
}


namespace Kernel {

	static Utcb _roottask_utcb, _idle_utcb;

	void _roottask_thread__verbose__creation(addr_t vip, addr_t vsp, Utcb* vutcb)
	{
		if (!KERNEL__VERBOSE) return;
		printf("Kernel::roottask_thread, roottask thread created, "
		       "printing constraints\n");
		printf("  vip=0x%8X, vsp=0x%8X, vutcb=0x%8X\n",
		       (uint32_t)vip, (uint32_t)vsp, (uint32_t)vutcb);
	}


	void idle()
	{
		while(1);
	}


	Thread *idle_thread()
	{
		enum{
			IDLE_STACK_WORD_SIZE=32,
			IDLE_TID=1,
		};

		static word_t    _it_stack[IDLE_STACK_WORD_SIZE];
		static Thread *_it = thread_factory()->get(IDLE_TID);

		if (!_it) {

			Thread_factory::Create_argument itca;

			itca.tid           = (Thread_id)IDLE_TID;
			itca.pid           = (Protection_id)Roottask::PROTECTION_ID;
			itca.utcb          = &_idle_utcb;
			itca.pager_tid     = INVALID_THREAD_ID;
			itca.vsp           = (addr_t)&LAST_ARRAY_ELEM(_it_stack);
			itca.vip           = (addr_t)&idle;
			itca.is_privileged = true;

			_it = thread_factory()->create(&itca, true);
		}
		return _it;
	}


	Thread *roottask_thread()
	{
		static word_t  _rt_stack[Roottask::MAIN_STACK_SIZE/WORD_SIZE];
		static Thread *_rt = thread_factory()->get(Roottask::MAIN_THREAD_ID);

		if (!_rt) {

			Thread_factory::Create_argument rtca;

			rtca.tid           = (Thread_id)Roottask::MAIN_THREAD_ID;
			rtca.pid           = (Protection_id)Roottask::PROTECTION_ID;
			rtca.utcb          = &_roottask_utcb;
			rtca.pager_tid     = INVALID_THREAD_ID;
			rtca.vsp           = (addr_t)&LAST_ARRAY_ELEM(_rt_stack);
			rtca.vip           = (addr_t)&ROOTTASK_ENTRY;
			rtca.is_privileged = true;
			_main_utcb_addr    = rtca.utcb;

			_rt = thread_factory()->create(&rtca, false);
			if (_rt)
				_roottask_thread__verbose__creation(
					rtca.vip, rtca.vsp, rtca.utcb);
		}
		return _rt;
	}
}


Platform *Kernel::platform() { static Platform _p; return &_p; }


Scheduler *Kernel::scheduler()
{
	static bool _init_scheduler = false;
	static Scheduler _s = Scheduler(platform(),
		                            platform()->timer(),
		                            idle_thread());
	if(_init_scheduler){ return &_s; }
	_s.add(roottask_thread());
	_init_scheduler = true;
	return &_s;
}


Tlb *Kernel::tlb() { return platform()->tlb(); }


Irq_controller * const Kernel::irq_controller()
{
	return platform()->irq_controller();
}


Irq_allocator * const Kernel::irq_allocator()
{
	static Irq_allocator _ia =
		Irq_allocator(platform()->irq_controller());
	return &_ia;
}


unsigned Kernel::word_width() { return Platform::WORD_WIDTH; }


void Kernel::halt() { platform()->halt(); }


Kernel::Kernel_entry *Kernel::kernel_entry_event()
{
	static Kernel_entry _ke;
	return &_ke;
}


Kernel::Kernel_exit *Kernel::kernel_exit_event()
{
	static Kernel_exit _kx;
	return &_kx;
}


/**
 * Kernel main routine, gets called by crt0_kernel.s
 */
extern "C" void _kernel()
{
	kernel_entry_event()->on_occurence();

	scheduler()->run();

	kernel_exit_event()->on_occurence();
}

