/*
 * \brief  Wrapper for the Ada main program
 * \author Norman Feske
 * \date   2009-09-23
 */

/* Genode includes */
#include <base/log.h>
#include <base/component.h>

/* local includes */
#include <machinery.h>

/**
 * Declaration of the Ada main procedure
 */
extern "C" void _ada_main(void);
extern "C" void add_package__add(int, int, int*);

/**
 * Make the linker happy
 */
extern "C" void __gnat_eh_personality()
{
	Genode::warning(__func__, " not implemented");
}

extern "C" void __gnat_rcheck_CE_Overflow_Check()
{
    Genode::warning(__func__, " not implemented");
}

extern "C" void adainit();
extern "C" void adafinal();

/**
 * Wrapper for the Ada main program
 *
 * This function is called on Genode component startup. It may be used to
 * initialize memory objects at fixed virtual addresses prior calling the Ada
 * main program.
 */
void Component::construct(Genode::Env &env)
{
	adainit();
	_ada_main();

	test_spark_object_construction();
	adafinal();

	env.parent().exit(0);
}
