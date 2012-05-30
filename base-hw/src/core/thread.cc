/*
 * \brief  Implementation of Thread API interface for core
 * \author Martin Stein
 * \date   2012-01-25
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/env.h>
#include <kernel/log.h>

/* core includes */
#include <platform.h>
#include <platform_thread.h>

using namespace Genode;

extern Genode::Native_utcb * _main_utcb;

namespace Kernel { unsigned core_id(); }


Native_utcb * Thread_base::utcb()
{
	/* this is the main thread */
	if (!this) { return _main_utcb; }

	/* this isn't the main thread */
	return _tid->phys_utcb();
}


/**
 * Returns 0 if this is the main thread or the thread base pointer otherwise
 */
Thread_base * Thread_base::myself()
{
	/* get our platform thread wich holds our thread base or 0 */
	Platform_thread * const pt = Kernel::get_thread();
	if (pt) return pt->thread_base();

	/* we are core main, the only thread beside idle with no platform thread */
	else return 0;
}


static void thread_entry()
{
	/* this is never called by a main thread */
	Thread_base::myself()->entry();
}


Thread_base::Thread_base(const char *name, size_t stack_size)
: _list_element(this), _tid(0)
{
	_tid = new (platform()->core_mem_alloc())
		Platform_thread(name, this, stack_size, Kernel::core_id());
}


Thread_base::~Thread_base()
{
	kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
	while (1) ;
}


void Thread_base::start()
{
	size_t const stack_size = _tid->stack_size()/sizeof(unsigned long) + 1;
	void * const stack_base = new (platform()->core_mem_alloc())
	                          unsigned long [stack_size];
	void * sp = (void *)((addr_t)stack_base + _tid->stack_size());
	void * ip = (void *)&thread_entry;
	if (_tid->start(ip, sp)) PERR("Couldn't start thread");
}


void Thread_base::cancel_blocking()
{
	kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
	while (1) ;
}

