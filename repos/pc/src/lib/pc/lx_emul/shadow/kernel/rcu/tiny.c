/*
 * \brief  Replaces kernel/rcu/tiny.c
 * \author Josef Soentgen
 * \date   2022-04-05
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#include <linux/mm.h>
#include <linux/rcupdate.h>

void call_rcu(struct rcu_head * head,rcu_callback_t func)
{
	/*
	 * In case func is a small offset kvfree should be
	 * called directly, see 'rcu_reclaim_tiny'.
	 */
	enum { KVFREE_RCU_OFFSET = 4096, };
	if (func < (rcu_callback_t)KVFREE_RCU_OFFSET) {
		kvfree((void*)head - (unsigned long)func);
		return;
	}

	func(head);
}
