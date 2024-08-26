/*
 * \brief  Replaces kernel/sched/core.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#define CREATE_TRACE_POINTS
#include <trace/events/sched.h>
#undef CREATE_TRACE_POINTS

#include <asm/preempt.h>
#include <linux/preempt.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <linux/sched/stat.h>
#include <linux/sched/nohz.h>
#include <linux/version.h>
#include <linux/kernel_stat.h>

#include <lx_emul/debug.h>
#include <lx_emul/task.h>
#include <lx_emul/time.h>

#include <linux/sched/cputime.h>
#include <linux/sched/clock.h>
#include <linux/sched/wake_q.h>
#include <../kernel/sched/sched.h>


struct rq runqueues;

/*
 * Type changes between kernel versions
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,14,0)
typedef unsigned long nr_iowait_cpu_return_t;
typedef long          wait_task_inactive_match_state_t;
#else
typedef unsigned int  nr_iowait_cpu_return_t;
typedef unsigned int  wait_task_inactive_match_state_t;
#endif


DEFINE_PER_CPU(struct kernel_stat, kstat);
EXPORT_PER_CPU_SYMBOL(kstat);


void set_user_nice(struct task_struct * p, long nice)
{
	p->static_prio = NICE_TO_PRIO(nice);
	p->prio        = p->static_prio;
	p->normal_prio = p->static_prio;
	lx_emul_task_priority(p, p->static_prio);
}


int
try_to_wake_up(struct task_struct *p, unsigned int state, int wake_flags)
{
	if (!p) lx_emul_trace_and_stop(__func__);
	if (!(p->__state & state))
		return 0;

	if (p != lx_emul_task_get_current())
		lx_emul_task_unblock(p);

	p->__state = TASK_RUNNING;

	return 1;
}


int wake_up_process(struct task_struct * p)
{
	return try_to_wake_up(p, TASK_NORMAL, 0);
}


int default_wake_function(wait_queue_entry_t *curr,
                          unsigned mode,
                          int wake_flags,
                          void *key)
{
	return try_to_wake_up(curr->private, mode, wake_flags);
}


static void __schedule(void)
{
	if (preempt_count()) {
		printk("schedule: unexpected preempt_count=%d\n", preempt_count());
		lx_emul_trace_and_stop("abort");
	}

	lx_emul_task_schedule(current->__state != TASK_RUNNING);
}

#include "../kernel/workqueue_internal.h"

asmlinkage __visible void __sched schedule(void)
{
	lx_emul_time_update_jiffies();

	if (current->__state) {
		unsigned int task_flags = current->flags;
		if (task_flags & PF_WQ_WORKER)
			wq_worker_sleeping(current);
	}

	__schedule();

	if (current->flags & PF_WQ_WORKER)
		wq_worker_running(current);
}


#ifdef CONFIG_DEBUG_PREEMPT

void preempt_count_add(int val)
{
	__preempt_count_add(val);
}


void preempt_count_sub(int val)
{
	__preempt_count_sub(val);
}

#endif /* CONFIG_DEBUG_PREEMPT */


asmlinkage __visible void __sched notrace preempt_schedule(void)
{
	if (likely(!preemptible()))
		return;

	lx_emul_time_update_jiffies();
	__schedule();
}


asmlinkage __visible void __sched notrace preempt_schedule_notrace(void)
{
	if (likely(!preemptible()))
		return;

	lx_emul_time_update_jiffies();
	__schedule();
}


nr_iowait_cpu_return_t nr_iowait_cpu(int cpu)
{
	return 0;
}


void scheduler_tick(void)
{
	sched_clock_tick();
}


void __sched schedule_preempt_disabled(void)
{
	lx_emul_task_schedule(current->__state != TASK_RUNNING);
}


int sched_setscheduler_nocheck(struct task_struct * p, int policy,
                               const struct sched_param * param)
{
	return 0;
}


int wake_up_state(struct task_struct * p, unsigned int state)
{
	p->__state = TASK_RUNNING;
	lx_emul_task_unblock(p);
	return 0;
}


/* Linux 6.4+ uses full-fat wait_task_inactive for the UP case */
#if defined CONFIG_SMP || LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)
unsigned long wait_task_inactive(struct task_struct * p,
                                 wait_task_inactive_match_state_t match_state)
{
	struct rq *rq = task_rq(p);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,1,0)
	if (task_running(rq, p))
#else
	if (task_on_cpu(rq, p))
#endif
		schedule();

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,1,0)
	if (task_running(rq, p))
#else
	if (task_on_cpu(rq, p))
#endif
		return 0;

	return 1;
}
#endif /* CONFIG_SMP || LINUX_VERSION_CODE */

#ifdef CONFIG_SMP
int set_cpus_allowed_ptr(struct task_struct * p,
                         const struct cpumask * new_mask)
{
	return 0;
}


void do_set_cpus_allowed(struct task_struct * p,
                         const struct cpumask * new_mask) { }


#ifdef CONFIG_NO_HZ_COMMON
int get_nohz_timer_target(void)
{
	return 0;
}


void wake_up_nohz_cpu(int cpu) { }
#endif /* CONFIG_NO_HZ_COMMON */
#endif /* CONFIG_SMP */


static bool __wake_q_add(struct wake_q_head *head, struct task_struct *task)
{
	struct wake_q_node *node = &task->wake_q;

	smp_mb__before_atomic();
	if (unlikely(cmpxchg_relaxed(&node->next, NULL, WAKE_Q_TAIL)))
		return false;

	*head->lastp = node;
	head->lastp = &node->next;
	return true;
}


void wake_q_add(struct wake_q_head *head, struct task_struct *task)
{
	if (__wake_q_add(head, task))
		get_task_struct(task);
}


/*
 * CAUTION: This check is not an actual requirement. It should be removed when
 * all other *_linux have been updated to 6.6 or when this function has been
 * removed from their respective generated_dummies.c
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,6,0)
void wake_q_add_safe(struct wake_q_head *head, struct task_struct *task)
{
	if (!__wake_q_add(head, task))
		put_task_struct(task);
}
#endif

void wake_up_q(struct wake_q_head *head)
{
	struct wake_q_node *node = head->first;

	while (node != WAKE_Q_TAIL) {
		struct task_struct *task;

		task = container_of(node, struct task_struct, wake_q);
		node = node->next;
		task->wake_q.next = NULL;

		wake_up_process(task);
		put_task_struct(task);
	}
}


int idle_cpu(int cpu)
{
	return 1;
}


void sched_set_fifo(struct task_struct * p) { }
