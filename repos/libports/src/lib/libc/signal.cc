/*
 * \brief  POSIX signals
 * \author Emery Hemingway
 * \date   2015-10-30
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libc includes */
extern "C" {
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
}


extern "C" int __attribute__((weak)) sigprocmask(int how, const sigset_t *set, sigset_t *old_set)
{
	/* no signals should be expected, so report all signals blocked */
	if (old_set != NULL)
		sigfillset(old_set);

	if (set == NULL)
		return 0;

	switch (how) {
	case SIG_BLOCK:   return 0;
	case SIG_SETMASK: return 0;
	case SIG_UNBLOCK: return 0;
	}
	errno = EINVAL;
	return -1;
}

extern "C" int __attribute__((weak)) _sigprocmask(int how, const sigset_t *set, sigset_t *old_set)
{
	return sigprocmask(how, set, old_set);
}
