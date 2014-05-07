#include <ipxe/rdtsc_timer.h>

static inline unsigned long currticks ( void )
{
	return __rdtsc_currticks();
}
