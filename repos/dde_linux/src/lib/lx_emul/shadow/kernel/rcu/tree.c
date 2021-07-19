/*
 * \brief  Replaces kernel/rcu/tree.c.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/types.h>

extern void __rcu_read_lock(void);
void __rcu_read_lock(void) { }


extern void __rcu_read_unlock(void);
void __rcu_read_unlock(void) { }

int rcu_needs_cpu(u64 basemono, u64 *nextevt)
{
	return 0;
}
