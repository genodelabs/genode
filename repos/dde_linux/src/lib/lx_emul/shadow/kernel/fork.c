/*
 * \brief  Replaces kernel/fork.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/refcount.h>
#include <linux/list.h>
#include <asm/processor.h>
#include <lx_emul/task.h>
#include <linux/sched/task.h>
#include <linux/cred.h>
#include <linux/sched/signal.h>

/*
 * We accept that we transfer the 4KiB task_struct object via stack in
 * initialization, therefore mask the warning here
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-larger-than="

pid_t kernel_thread(int (* fn)(void *),void * arg,unsigned long flags)
{
	static int pid_counter = FIRST_PID;

	struct cred * cred;
	struct task_struct * task;
	struct signal_struct *signal;

	cred = kzalloc(sizeof (struct cred), GFP_KERNEL);
	if (!cred)
		return -1;

	signal = kzalloc(sizeof(struct signal_struct), GFP_KERNEL);
	if (!signal)
		goto err_signal;

	task = kmalloc(sizeof(struct task_struct), GFP_KERNEL);
	if (!task)
		goto err_task;

	*task = (struct task_struct) {
	.__state         = 0,
	.usage           = REFCOUNT_INIT(2),
	.flags           = PF_KTHREAD,
	.prio            = MAX_PRIO - 20,
	.static_prio     = MAX_PRIO - 20,
	.normal_prio     = MAX_PRIO - 20,
	.policy          = SCHED_NORMAL,
	.cpus_ptr        = &task->cpus_mask,
	.cpus_mask       = CPU_MASK_ALL,
	.nr_cpus_allowed = 1,
	.mm              = NULL,
	.active_mm       = NULL,
	.tasks           = LIST_HEAD_INIT(task->tasks),
	.real_parent     = lx_emul_task_get_current(),
	.parent          = lx_emul_task_get_current(),
	.children        = LIST_HEAD_INIT(task->children),
	.sibling         = LIST_HEAD_INIT(task->sibling),
	.group_leader    = task,
	.thread          = INIT_THREAD,
	.blocked         = {{0}},
	.pid             = pid_counter++,
	.pending         = {
		.list   = LIST_HEAD_INIT(task->pending.list),
		.signal = {{0}} },
	.cred            = cred,
	.signal          = signal,
	};

#ifndef CONFIG_THREAD_INFO_IN_TASK
	/* On arm, the 'thread_info' is hidden behind 'task->stack', we must
	 * therefore initialise the member before calling 'task_thread_info()'. */
	task->stack = kmalloc(sizeof(struct thread_info), THREADINFO_GFP);
#endif

#ifndef CONFIG_X86
	task_thread_info(task)->preempt_count = 0;
#endif

	lx_emul_task_create(task, "kthread", task->pid, fn, arg);

#ifdef CONFIG_THREAD_INFO_IN_TASK
	task->stack = lx_emul_task_stack(task);
#endif

	return task->pid;

err_task:
	kfree(signal);
err_signal:
	kfree(cred);
	return -1;
}

#pragma GCC diagnostic pop
