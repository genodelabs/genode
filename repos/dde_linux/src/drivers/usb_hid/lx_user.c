/*
 * \brief  Post kernel activity
 * \author Sebastian Sumpf
 * \date   2023-06-29
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/sched/task.h>
#include <usb_hid.h>

static struct task_struct *main_task = NULL;


struct task_struct *lx_user_new_usb_task(int (*func)(void*), void *args)
{
	int pid = kernel_thread(func, args, CLONE_FS | CLONE_FILES);
	return find_task_by_pid_ns(pid, NULL);
}


void lx_user_destroy_usb_task(struct task_struct *task)
{
	if (task != current) {
		printk("%s: task: %px is not current: %px\n", __func__,
		       task, current);
		return;
	}

	/* unblock main task which initiated destruction */
	lx_emul_task_unblock(main_task);

	do_exit(0);
}


void lx_user_init(void)
{
	int pid = kernel_thread(lx_user_main_task, &main_task, CLONE_FS | CLONE_FILES);
	main_task = find_task_by_pid_ns(pid, NULL);
}
