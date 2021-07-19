/*
 * \brief  Lx_emul task backend
 * \author Stefan Kalkowski
 * \date   2021-05-05
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul/task.h>
#include <lx_kit/env.h>
#include <lx_kit/task.h>

extern "C" struct task_struct * lx_emul_task_get_current(void)
{
	return (struct task_struct*)
		Lx_kit::env().scheduler.current().lx_task();
}


extern "C"
void lx_emul_task_create(struct task_struct * task,
                         const char         * name,
                         int                  pid,
                         int               (* threadfn)(void * data),
                         void               * data)
{
	new (Lx_kit::env().heap) Lx_kit::Task(threadfn,
	                                      data,
	                                      (void*)task, pid, name,
	                                      Lx_kit::env().scheduler,
	                                      Lx_kit::Task::NORMAL);
}


extern "C" void lx_emul_task_unblock(struct task_struct * t)
{
	Lx_kit::env().scheduler.task((void*)t).unblock();
}


extern "C" void lx_emul_task_priority(struct task_struct * t,
                                      unsigned long prio)
{
	Lx_kit::env().scheduler.task((void*)t).priority(prio);
}


extern "C" void lx_emul_task_schedule(int block)
{
	Lx_kit::Task & task = Lx_kit::env().scheduler.current();
	if (block) task.block();
	task.schedule();
}


extern "C" struct task_struct * lx_emul_task_get(int pid)
{
	void * ret = nullptr;

	Lx_kit::env().scheduler.for_each_task([&] (Lx_kit::Task & task) {
		if (pid == task.pid())
			ret = task.lx_task();
	});

	return (task_struct*) ret;
}


extern "C" void lx_emul_task_name(struct task_struct * t, const char * name)
{
	Lx_kit::env().scheduler.task((void*)t).name(name);
}
