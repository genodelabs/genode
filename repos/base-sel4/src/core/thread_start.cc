/*
 * \brief  Implementation of Thread API interface for core
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/printf.h>
#include <base/sleep.h>

/* core includes */
#include <platform.h>
#include <platform_thread.h>

using namespace Genode;


void Thread_base::_init_platform_thread(size_t, Type type)
{
	PDBG("not implemented");
}


void Thread_base::_deinit_platform_thread()
{
	PDBG("not implemented");
}


void Thread_base::_thread_start()
{
	Thread_base::myself()->_thread_bootstrap();
	Thread_base::myself()->entry();
	sleep_forever();
}


void Thread_base::start()
{
	PDBG("not implemented");
}


void Thread_base::cancel_blocking()
{
	PWRN("not implemented");
}

