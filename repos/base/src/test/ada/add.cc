/*
 * \brief  C functions referenced by Ada code
 * \author Norman Feske
 * \date   2009-09-23
 */

/* Genode includes */
#include <base/log.h>

using Genode::log;


extern "C" void add(int a, int b, int *result)
{
	log("add called with a=", a, ", b=", b, ", result at address ",
	     (void*)result);
	*result = a + b;
}


extern "C" void print_int(int a)
{
	log("print_int called with argument ", a);
}
