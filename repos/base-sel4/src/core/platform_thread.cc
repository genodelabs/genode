/*
 * \brief   Thread facility
 * \author  Norman Feske
 * \date    2015-05-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <util/string.h>

/* core includes */
#include <platform_thread.h>

using namespace Genode;


int Platform_thread::start(void *ip, void *sp, unsigned int cpu_no)
{
	PDBG("not implemented");
	return 0;
}


void Platform_thread::pause()
{
	PDBG("not implemented");
}


void Platform_thread::resume()
{
	PDBG("not implemented");
}


void Platform_thread::state(Thread_state s)
{
	PDBG("Not implemented");
	throw Cpu_session::State_access_failed();
}


Thread_state Platform_thread::state()
{
	PDBG("Not implemented");
	throw Cpu_session::State_access_failed();
}


void Platform_thread::cancel_blocking()
{
	PDBG("not implemented");
}


Weak_ptr<Address_space> Platform_thread::address_space()
{
	return _address_space;
}


Platform_thread::Platform_thread(size_t, const char *name, unsigned, addr_t)
{
	PDBG("not implemented");
}


Platform_thread::~Platform_thread()
{
	PDBG("not implemented");
}
