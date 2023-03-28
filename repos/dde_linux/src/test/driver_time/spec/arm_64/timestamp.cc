#include <trace/timestamp.h>

extern "C" unsigned long long lx_emul_timestamp();
extern "C" unsigned long long lx_emul_timestamp()
{
	return Genode::Trace::timestamp();
}
