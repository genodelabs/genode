/*
 * \brief  C-library back end
 * \author Christian Prochaska
 * \date   2009-07-20
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/sleep.h>

/* libc includes */
#include <libc/component.h>

/* libc-internal includes */
#include <internal/atexit.h>

using namespace Libc;


static Libc::Atexit *_atexit_ptr;


void Libc::init_atexit(Atexit &atexit)
{
	_atexit_ptr = &atexit;
}


void Libc::execute_atexit_handlers_in_application_context()
{
	if (!_atexit_ptr) {
		error("missing call of 'init_atexit'");
		sleep_forever();
	}

	/*
	 * The atexit handlers must be executed in application context because the
	 * handlers may perform I/O such as the closing of file descriptors.
	 */
	with_libc([&] () {
		_atexit_ptr->execute_handlers(nullptr);
	});
}


/*********************
 ** CXA ABI support **
 *********************/

/*
 * The C++ compiler generates calls to '__cxa_atexit' for each object that
 * is instantiated as static local variable.
 */

extern "C" int __cxa_atexit(void (*func)(void*), void *arg, void *dso)
{
	if (!_atexit_ptr)
		return -1;

	_atexit_ptr->register_cxa_handler(func, arg, dso);
	return 0;
}


extern "C" int __aeabi_atexit(void *arg, void(*func)(void*), void *dso)
{
	return __cxa_atexit(func, arg, dso);
}


extern "C" void __cxa_finalize(void *dso)
{
	execute_atexit_handlers_in_application_context();
}


extern "C" int atexit(void (*func)(void))
{
	_atexit_ptr->register_std_handler(func);
	return 0;
}

