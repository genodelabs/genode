#include <lx/extern_c_begin.h>
#include <lx/lx.h>
#include <lx/extern_c_end.h>

#include <base/printf.h>

void lx_printf(char const *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	Genode::vprintf(fmt, va);
	va_end(va);
}


void lx_vprintf(char const *fmt, va_list va) {
	Genode::vprintf(fmt, va); }
