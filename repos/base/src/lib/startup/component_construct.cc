/*
 * \brief   Startup code for component construction
 * \author  Christian Helmuth
 * \date    2016-01-21
 *
 * The component construction code is used by the startup library, which is
 * linked to static binaries and ld.lib.so. The code is also used by the
 * component_entry_point static library, which is linked to all dynamic
 * executables to make the fallback implementation and the
 * call_component_construct-hook function pointer available to these binaries.
 *
 * Note, for dynamic binaries we can't refer to the default implementation in
 * ld.lib.so as it is a component itself implementing the Component functions.
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/component.h>


namespace Genode {

	/*
	 * Hook for intercepting the call of the 'Component::construct' method. By
	 * hooking this function pointer in a library constructor, the libc is able
	 * to create a task context for the component code. This context is
	 * scheduled by the libc in a cooperative fashion, i.e. when the
	 * component's entrypoint is activated.
	 */

	extern void (*call_component_construct)(Genode::Env &) __attribute__((weak));
}

static void default_component_construct(Genode::Env &env)
{
	Component::construct(env);
}

void (*Genode::call_component_construct)(Genode::Env &) = &default_component_construct;


/****************************************************
 ** Fallback implementation of Component interface **
 ****************************************************/

extern void genode_exit(int status);

static int exit_status;

static void exit_on_suspended() { genode_exit(exit_status); }

/*
 * Regular components provide the 'Component' interface as defined in
 * base/component.h. This fallback accommodates legacy components that lack the
 * implementation of this interface but come with a main function.
 */

/*
 * XXX these symbols reside in the startup library - candidate for
 * base-internal header?
 */
extern char **genode_argv;
extern int    genode_argc;
extern char **genode_envp;

extern int main(int argc, char **argv, char **envp);

void Component::construct(Genode::Env &env) __attribute__((weak));
void Component::construct(Genode::Env &env)
{
	/* call real main function */
	exit_status = main(genode_argc, genode_argv, genode_envp);

	/* trigger suspend in the entry point */
	env.ep().schedule_suspend(exit_on_suspended, nullptr);

	/* return to entrypoint and exit via exit_on_suspended() */
}


Genode::size_t Component::stack_size() __attribute__((weak));
Genode::size_t Component::stack_size()
{
	return 16UL * 1024 * sizeof(Genode::addr_t);
}
