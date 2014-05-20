#include <base/printf.h>
#include "test-ldso.h"

extern "C" void lib_dl_symbol()
{
	Genode::printf("called (from '%s')\n", __func__);
	Genode::printf("Call 'lib_1_good': ");
	lib_1_good();
}
