/*
 * \brief  Lock implementation
 * \author Norman Feske
 * \date   2007-10-15
 */

/*
 * Copyright (C) 2007-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/cancelable_lock.h>
#include <base/printf.h>
#include <cpu/atomic.h>

/* Codezero includes */
#include <codezero/syscalls.h>

using namespace Genode;


Cancelable_lock::Cancelable_lock(Cancelable_lock::State initial)
:
	_native_lock(UNLOCKED)
{
	if (initial == LOCKED)
		lock();
}


void Cancelable_lock::lock()
{
	while (!cmpxchg(&_native_lock, UNLOCKED, LOCKED))
		Codezero::l4_thread_switch(-1);
}


void Cancelable_lock::unlock()
{
	_native_lock = UNLOCKED;
}


/*
 * Printf implementation to make Codezero's syscall bindings happy.
 *
 * We need a better place for this function - actually, the best would be not
 * to need this function at all. As of now, 'printf' is referenced by
 * Codezero's libl4, in particular by the mutex implementation.
 */
extern "C" void printf(const char *format, ...) __attribute__((weak));
extern "C" void printf(const char *format, ...)
{
	va_list list;
	va_start(list, format);

	vprintf(format, list);

	va_end(list);
}
