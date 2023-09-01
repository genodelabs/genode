/*
 * \brief  Post kernel activity
 * \author Sebastian Sumpf
 * \date   2024-01-29
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/sched/task.h>
#include <linux/init.h>

#include "lx_user.h"


struct task_struct *lx_user_new_task(int (*func)(void*), void *args)
{
	int pid = kernel_thread(func, args, CLONE_FS | CLONE_FILES);
	return find_task_by_pid_ns(pid, NULL);
}


void lx_user_destroy_task(struct task_struct *task)
{
	if (task != current) {
		printk("%s: task: %px is not current: %px\n", __func__,
		       task, current);
		return;
	}

	do_exit(0);
}


static int _startup_finished = 0;

int lx_user_startup_complete(void *) { return _startup_finished; }


static struct task_struct *_socket_dispatch_root = NULL;

struct task_struct *lx_socket_dispatch_root(void)
{
	return _socket_dispatch_root;
}


int __setup_set_thash_entries(char const *str);
int __setup_set_uhash_entries(char const *str);

void lx_user_configure_ip_stack(void)
{
	__setup_set_thash_entries("2048");
	__setup_set_uhash_entries("2048");
}


void lx_user_init(void)
{
	_socket_dispatch_root = lx_user_new_task(lx_socket_dispatch,
	                                         lx_socket_dispatch_queue());
	_startup_finished = 1;
}
