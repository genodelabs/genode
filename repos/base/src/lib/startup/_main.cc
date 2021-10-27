/*
 * \brief   Startup code
 * \author  Christian Helmuth
 * \author  Christian Prochaska
 * \author  Norman Feske
 * \date    2006-04-12
 *
 * The startup code calls constructors for static objects before calling
 * main(). Furthermore, this file contains the support of exit handlers
 * and destructors.
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/sleep.h>
#include <base/component.h>
#include <deprecated/env.h>

/* platform-specific local helper functions */
#include <base/internal/parent_cap.h>
#include <base/internal/crt0.h>

void * __dso_handle = 0;


/**
 * Dummy default arguments for main function
 */
static char  argv0[] = { '_', 'm', 'a', 'i', 'n', 0};
static char *argv[1] = { argv0 };


/**
 * Arguments for main function
 *
 * These global variables may be initialized by a constructor provided by an
 * external library.
 */
char **genode_argv = argv;
int    genode_argc = 1;
char **genode_envp = 0;


/******************************************************
 ** C entry function called by the crt0 startup code **
 ******************************************************/


namespace Genode {

	/*
	 * To be called from the context of the initial entrypoiny before
	 * passing control to the 'Component::construct' function.
	 */
	void call_global_static_constructors()
	{
		/* Don't do anything if there are no constructors to call */
		addr_t const ctors_size = (addr_t)&_ctors_end - (addr_t)&_ctors_start;
		if (ctors_size == 0)
			return;

		void (**func)();
		for (func = &_ctors_end; func != &_ctors_start; (*--func)());
	}

	/* XXX move to base-internal header */
	extern void bootstrap_component();
}


extern "C" int _main()
{
	Genode::bootstrap_component();

	/* never reached */
	return 0;
}


extern int main(int argc, char **argv, char **envp);


void Component::construct(Genode::Env &env) __attribute__((weak));
void Component::construct(Genode::Env &)
{
	/* call real main function */
	main(genode_argc, genode_argv, genode_envp);
}

