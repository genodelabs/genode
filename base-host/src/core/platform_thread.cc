/*
 * \brief   Thread facility
 * \author  Norman Feske
 * \date    2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* core includes */
#include <platform_thread.h>

using namespace Genode;


void Platform_thread::affinity(unsigned int cpu_no)
{
	PERR("not yet implemented");
}


int Platform_thread::start(void *ip, void *sp, unsigned int cpu_no)
{
	PWRN("not implemented");
	return -1;
}


void Platform_thread::pause()
{
	PWRN("not implemented");
}


void Platform_thread::resume()
{
	PWRN("not implemented");
}


int Platform_thread::state(Thread_state *state_dst)
{
	PWRN("not implemented");
	return -1;
}


void Platform_thread::cancel_blocking()
{
	PWRN("not implemented");
}


unsigned long Platform_thread::pager_object_badge() const
{
	PWRN("not implemented");
	return -1;
}


Platform_thread::Platform_thread(const char *name, unsigned, addr_t,
                                 int thread_id)
{
	PWRN("not implemented");
}


Platform_thread::~Platform_thread()
{
	PWRN("not implemented");
}
