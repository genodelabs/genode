/*
 * \brief  C functions referenced by Ada code
 * \author Norman Feske
 * \date   2009-09-23
 */

/* Genode includes */
#include <base/printf.h>

using namespace Genode;


extern "C" void add(int a, int b, int *result)
{
	printf("add called with a=%d, b=%d, result at address 0x%p\n",
	       a, b, result);
	*result = a + b;
}


extern "C" void print_int(int a)
{
	printf("print_int called with argument %d\n", a);
}
