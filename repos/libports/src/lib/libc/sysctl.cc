/*
 * \brief  C-library back end
 * \author Norman Feske
 * \date   2008-11-11
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <errno.h>

#include "libc_debug.h"

extern "C" int __sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
                        void *newp, size_t newlen)
{
	int i;
	raw_write_str("__sysctl called\n");

	/*
	 * During printf, __sysctl is called by malloc with the parameters
	 *
	 * name[0] == 6 (CTL_HW)
	 * name[1] == 3 (HW_NCPU)
	 *
	 * In this case, we can return -1, and the libC assumes that we
	 * have one CPU.
	 */
	if ((name[0] == CTL_HW) && (name[1] == HW_NCPU))
		return -1;

	/*
	 * CTL_P1003_1B is used by sysconf(_SC_PAGESIZE) to determine
	 * the actual page size.
	 */
	if (((name[0] == CTL_HW) && (name[1] == HW_PAGESIZE)) ||
	    ((name[0] == CTL_P1003_1B) && (name[1] == CTL_P1003_1B_PAGESIZE))) {
		int result = 4096;
		if (oldp) {
			if (*oldlenp >= sizeof(result)) {
				*(int*)oldp = result;
				*oldlenp = sizeof(result);
				return 0;
			} else {
				PERR("not enough room to store the old value");
				return -1;
			}
		} else {
			PERR("cannot store the result in oldp, since it is NULL");
			return -1;
		}
	}

	raw_write_str(newp ? "newp provided\n" : "no newp provided\n");

	for (i = 0; i < name[0]; i++)
		raw_write_str("*");
	raw_write_str("\n");
	for (i = 0; i < name[1]; i++)
		raw_write_str("%");
	raw_write_str("\n");

	return 0;
}

