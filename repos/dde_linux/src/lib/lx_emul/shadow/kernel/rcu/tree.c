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

#include <linux/cache.h>
#include <linux/types.h>
#include <linux/time64.h> /* KTIME_MAX */
#include <linux/mm.h>
#include <linux/rcutree.h>
#include <linux/rcupdate.h>
#include <linux/version.h>

#ifdef CONFIG_PREEMPT_RCU
void __rcu_read_lock(void) { }


void __rcu_read_unlock(void) { }
#endif

extern void rcu_read_unlock_strict(void);
void rcu_read_unlock_strict(void) { }


#if LINUX_VERSION_CODE < KERNEL_VERSION(5,18,0)
int rcu_needs_cpu(u64 basemono, u64 *nextevt)
{
	if (nextevt)
		*nextevt = KTIME_MAX;
#else
int rcu_needs_cpu(void)
{
#endif
	return 0;
}


noinstr void rcu_irq_enter(void) { }


void noinstr rcu_irq_exit(void) { }


void rcu_softirq_qs(void) { }


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


void kvfree_call_rcu(struct rcu_head * head, rcu_callback_t func)
{
	void *ptr;

	if (head) {
		ptr = (void *) head - (unsigned long) func;
	} else {
		/*
		 * (original Linux comment)
		 *
		 * Please note there is a limitation for the head-less
		 * variant, that is why there is a clear rule for such
		 * objects: it can be used from might_sleep() context
		 * only. For other places please embed an rcu_head to
		 * your data.
		 */
		might_sleep();
		ptr = (unsigned long *) func;
	}
	kvfree(ptr);
}
