/*
 * \brief  Wrapper for the Ada main program
 * \author Norman Feske
 * \date   2009-09-23
 */

/* Genode includes */
#include <base/log.h>

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
 * C wrapper for the Ada main program
 *
 * This function is called by the '_main' startup code. It may be used to
 * initialize memory objects at fixed virtual addresses prior calling the Ada
 * main program.
 */
extern "C" int main(int argc, char **argv)
{
	_ada_main();
	return 0;
}
