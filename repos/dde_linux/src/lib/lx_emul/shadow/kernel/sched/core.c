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

#include <lx_emul/debug.h>
#include <lx_emul/task.h>

#include <../kernel/sched/sched.h>


/*
 * Type changes between kernel versions
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,14,1)
typedef unsigned long nr_iowait_cpu_return_t;
typedef long          wait_task_inactive_match_state_t;
#else
typedef unsigned int  nr_iowait_cpu_return_t;
typedef unsigned int  wait_task_inactive_match_state_t;
#endif


void set_user_nice(struct task_struct * p, long nice)
{
	p->static_prio = NICE_TO_PRIO(nice);
	p->prio        = p->static_prio;
	p->normal_prio = p->static_prio;
	lx_emul_task_priority(p, p->static_prio);
}


int set_cpus_allowed_ptr(struct task_struct * p,
                         const struct cpumask * new_mask)
{
	return 0;
}


static int
try_to_wake_up(struct task_struct *p, unsigned int state, int wake_flags)
{
	if (!p) lx_emul_trace_and_stop(__func__);
	if (!(p->state & state))
		return 0;

	if (p != lx_emul_task_get_current())
		lx_emul_task_unblock(p);

	p->state = TASK_RUNNING;
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


asmlinkage __visible void __sched schedule(void)
{
	if (preempt_count()) {
		printk("schedule: unexpected preempt_count=%d\n", preempt_count());
		lx_emul_trace_and_stop("abort");
	}

	lx_emul_task_schedule(current->state != TASK_RUNNING);
}


#ifdef CONFIG_DEBUG_PREEMPT

void preempt_count_add(int val)
{
	current_thread_info()->preempt.count += val;
}


void preempt_count_sub(int val)
{
	current_thread_info()->preempt.count -= val;
}

#endif /* CONFIG_DEBUG_PREEMPT */


asmlinkage __visible void __sched notrace preempt_schedule(void)
{
	if (likely(!preemptible()))
		return;

	schedule();
}


asmlinkage __visible void __sched notrace preempt_schedule_notrace(void)
{
	if (likely(!preemptible()))
		return;

	schedule();
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
	lx_emul_task_schedule(current->state != TASK_RUNNING);
}


int sched_setscheduler_nocheck(struct task_struct * p, int policy,
                               const struct sched_param * param)
{
	return 0;
}


unsigned long wait_task_inactive(struct task_struct * p,
                                 wait_task_inactive_match_state_t match_state)
{
	struct rq *rq = task_rq(p);

	if (task_running(rq, p))
		schedule();

	if (task_running(rq, p))
		return 0;

	return 1;
}


int wake_up_state(struct task_struct * p, unsigned int state)
{
	p->state = TASK_RUNNING;
	lx_emul_task_unblock(p);
	return 0;
}


#ifdef CONFIG_SMP
#ifdef CONFIG_NO_HZ_COMMON

int get_nohz_timer_target(void)
{
	return 0;
}

#endif
#endif
