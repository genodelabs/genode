/*
 * \brief  Wrapper for the Ada main program
 * \author Norman Feske
 * \date   2009-09-23
 */

/* Genode includes */
#include <base/log.h>
#include <base/component.h>

/**
 * Declaration of the Ada main procedure
 */
extern "C" void _ada_main(void);

/**
 * Make the linker happy
 */
extern "C" void __gnat_eh_personality()
{
	Genode::warning(__func__, " not implemented");
}

/**
 * Wrapper for the Ada main program
 *
 * This function is called on Genode component startup. It may be used to
 * initialize memory objects at fixed virtual addresses prior calling the Ada
 * main program.
 */
void Component::construct(Genode::Env &env)
{
	_ada_main();

	env.parent().exit(0);
}
