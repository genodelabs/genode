/*
 * \brief  Replaces kernel/rcu/tiny.c for non-SMP
 * \author Sebastian Sumpf
 * \date   2024-02-01
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/cache.h>
#include <linux/types.h>
#include <linux/time64.h> /* KTIME_MAX */
#include <linux/mm.h>
#include <linux/rcutiny.h>
#include <linux/rcupdate.h>
#include <linux/version.h>


void call_rcu(struct rcu_head * head,
              rcu_callback_t func)
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


unsigned long get_state_synchronize_rcu(void)
{
	lx_emul_trace_and_stop(__func__);
}


unsigned long start_poll_synchronize_rcu(void)
{
	lx_emul_trace_and_stop(__func__);
}


bool poll_state_synchronize_rcu(unsigned long oldstate)
{
	lx_emul_trace_and_stop(__func__);
}


void rcu_qs(void) { }
