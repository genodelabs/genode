/*
 * \brief  Replaces fs/exec.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/sched.h>
#include <lx_emul/task.h>

void __set_task_comm(struct task_struct * tsk,const char * buf,bool exec)
{
	strlcpy(tsk->comm, buf, sizeof(tsk->comm));
	lx_emul_task_name(tsk, tsk->comm);
}
