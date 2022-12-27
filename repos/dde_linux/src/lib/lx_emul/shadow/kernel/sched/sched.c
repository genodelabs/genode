/*
 * \brief  Supplement for emulation of kernel/sched/core.c
 * \author Norman Feske
 * \date   2021-06-02
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/sched.h>

int __cond_resched(void)
{
	if (should_resched(0)) {
		schedule();
		return 1;
	}
	return 0;
}
