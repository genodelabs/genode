/*
 * \brief  Supplemental code for hybrid Genode/Linux programs
 * \author Norman Feske
 * \date   2011-09-02
 */

/*
 * Copyright (C) 2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/crt0.h>
#include <base/printf.h>
#include <_main_helper.h>


extern "C" int raw_write_str(const char *str);


/**
 * Dummy for symbol that is normally provided by '_main.cc'
 */
int genode___cxa_atexit(void (*func)(void*), void *arg, void *dso)
{
	raw_write_str("genode___cxa_atexit called, not implemented\n");
	return 0;
}


/*
 * Manually initialize the 'lx_environ' pointer. For non-hybrid programs, this
 * pointer is initialized by the startup code.
 */
extern char **environ;
extern char **lx_environ;

/*
 * This function must be called before any other static constructor in the Genode
 * application, so it gets the highest priority (lowest priority number >100)
 */
__attribute__((constructor(101))) void lx_hybrid_init()
{
	main_thread_bootstrap();
	lx_environ = environ;
}
