/*
 * \brief  C-library back end
 * \author Norman Feske
 * \date   2008-11-11
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* libc includes */
#include <sys/limits.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>

/* libc-internal includes */
#include <internal/fd_alloc.h>


extern "C" int __attribute__((weak)) getrlimit(int resource, struct rlimit *rlim)
{
	/*
	 * The pthread init code on Linux calls getrlimit
	 * for resource 3 (RLIMIT_STACK). In this case, we
	 * return unlimited.
	 */
	if (resource == 3) {
		rlim->rlim_cur = ~0;
		rlim->rlim_max = ~0;
		return 0;
	}

	/*
	 * Maximal size of address space
	 */
	if ((resource == RLIMIT_AS) || (resource == RLIMIT_DATA)) {
		rlim->rlim_cur = LONG_MAX;
		rlim->rlim_max = LONG_MAX;
		return 0;
	}

	/*
	 * Maximum number of file descriptors
	 */
	if (resource == RLIMIT_NOFILE) {
		rlim->rlim_cur = MAX_NUM_FDS;
		rlim->rlim_max = MAX_NUM_FDS;
		return 0;
	}

	Genode::warning(__func__, " called for unsupported resource ", resource, ", returning 0");

	rlim->rlim_cur = 0;
	rlim->rlim_max = 0;

	return 0;
}

