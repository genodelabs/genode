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


pid_t kernel_thread(int (* fn)(void *),void * arg,unsigned long flags)
{
	static int pid_counter = FIRST_PID;

	struct task_struct * task = kmalloc(sizeof(struct task_struct), GFP_KERNEL);
	*task = (struct task_struct){
	.state           = 0,
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
		.signal = {{0}}
	}};

	task->thread_info.preempt_count = 0;

	lx_emul_task_create(task, "kthread", task->pid, fn, arg);
	return task->pid;
}
