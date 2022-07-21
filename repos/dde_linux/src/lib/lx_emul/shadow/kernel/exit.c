/*
 * \brief  Replaces kernel/exit.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/rcuwait.h>

#include <lx_emul/debug.h>
#include <lx_emul/task.h>


int rcuwait_wake_up(struct rcuwait * w)
{
	if (w && w->task)
		return  wake_up_process(w->task);

	return 0;
}



void __noreturn do_exit(long code)
{
	struct task_struct *tsk = current;
	tsk->exit_code = code;
	set_special_state(TASK_DEAD);

	if (tsk->vfork_done)
		complete(tsk->vfork_done);

	current->flags |= PF_NOFREEZE;

	lx_emul_task_mark_for_removal(tsk);

	schedule();
	BUG();
}
