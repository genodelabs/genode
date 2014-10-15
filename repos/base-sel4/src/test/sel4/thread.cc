/*
 * \brief  Implementation of the Thread API
 * \author Norman Feske
 * \date   2014-10-15
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/thread.h>
#include <base/env.h>
#include <base/snprintf.h>
#include <util/string.h>
#include <util/misc_math.h>

using namespace Genode;


static Thread_base::Context *main_context()
{
	enum { STACK_SIZE = sizeof(long)*4*1024 };

	static long buffer[STACK_SIZE/sizeof(long)];

	/* the context is located beyond the top of the stack */
	addr_t const context_addr = (addr_t)buffer + sizeof(buffer)
	                          - sizeof(Thread_base::Context);

	Thread_base::Context *context = (Thread_base::Context *)context_addr;
	context->stack_base = (addr_t)buffer;
	return context;
}


Thread_base *Thread_base::myself() { return nullptr; }


Thread_base::Thread_base(size_t, const char *name, size_t stack_size, Type type,
                         Cpu_session *cpu_session)
:
	_cpu_session(cpu_session), _context(main_context())
{
	strncpy(_context->name, name, sizeof(_context->name));
	_context->thread_base = this;
}


Thread_base::Thread_base(size_t quota, const char *name, size_t stack_size, Type type)
: Thread_base(quota, name, stack_size, type, nullptr) { }


Thread_base::~Thread_base() { }
