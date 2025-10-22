/*
 * \brief  libsupc++ quirk for lx-hybrid components
 * \author Christian Helmuth
 * \date   2025-10-22
 *
 * With version 2.38 glibc introduced support for ISO C23 API functions and
 * among others versioned strtoul() providing 0b... number parsing if requested
 * on compile time. The libsupc++.a binary library from GCC uses strtoul().
 * Unfortunately, the old-stable Debian 12/bookworm (replaced only weeks ago by
 * 13/trixie) uses glibc version 2.36 while Ubuntu 24.04/noble uses 2.39.
 * Hence, bookworm lacks ISO C23 support and trixie/noble use it by default.
 *
 * As the current Ubuntu LTS is our primary develpment environment, we provide
 * the missing __isoc23_strtoul() to libsupc++ from lx_hybrid and locally use
 * the original strtoul() for backwards compatibility.
 */

#include <features.h>
#undef __GLIBC_USE_C2X_STRTOL
#undef __GLIBC_USE_C23_STRTOL
#define __GLIBC_USE_C2X_STRTOL  0
#define __GLIBC_USE_C23_STRTOL  0
#include <stdlib.h>

unsigned long int __isoc23_strtoul (const char *a0, char ** a1, int a2)
{
	return strtoul(a0, a1, a2);
}

