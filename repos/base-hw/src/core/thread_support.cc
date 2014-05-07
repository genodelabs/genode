/*
 * \brief  Implementation of Thread API interface for core
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2014-02-27
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/sleep.h>
#include <base/env.h>

/* core includes */
#include <platform.h>
#include <platform_thread.h>

using namespace Genode;

extern Genode::Native_utcb * _main_thread_utcb;

Native_utcb * main_thread_utcb() {
	return _main_thread_utcb; }


void Thread_base::start()
{
	/* start thread with stack pointer at the top of stack */
	if (_tid.platform_thread->start((void *)&_thread_start, stack_top()))
		PERR("failed to start thread");
}


void Thread_base::cancel_blocking()
{
	_tid.platform_thread->cancel_blocking();
}


void Thread_base::_deinit_platform_thread()
{
	/* destruct platform thread */
	destroy(platform()->core_mem_alloc(), _tid.platform_thread);
}


void Thread_base::_init_platform_thread(Type type)
{
	/* create platform thread */
	_tid.platform_thread = new (platform()->core_mem_alloc())
		Platform_thread(_context->name, &_context->utcb);

	if (type == NORMAL) { return; }

	PWRN("not implemented!");
}
