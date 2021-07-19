/*
 * \brief  Scheduler for executing Lx::Task objects
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/sleep.h>
#include <os/backtrace.h>

#include <lx_kit/scheduler.h>
#include <lx_kit/task.h>

using namespace Genode;
using namespace Lx_kit;

Task & Scheduler::current()
{
	if (!_current) {
		error("Lx_kit::Scheduler::_current is zero!");
		backtrace();
		sleep_forever();
	}

	return *_current;
}


bool Scheduler::active() const
{
	return _current != nullptr;
}


void Scheduler::add(Task & task)
{
	Task * prev = nullptr;
	Task * next = _present_list.first();

	for (; next; prev = next, next = prev->next()) {
		if (next->priority() >= task.priority())
			break;
	}
	_present_list.insert(&task, prev);
}


void Scheduler::remove(Task & task)
{
	_present_list.remove(&task);
}


void Scheduler::unblock_irq_handler()
{
	for (Task * t = _present_list.first(); t; t = t->next()) {
		if (t->type() == Task::IRQ_HANDLER) t->unblock();
	}
}


void Scheduler::unblock_time_handler()
{
	for (Task * t = _present_list.first(); t; t = t->next()) {
		if (t->type() == Task::TIME_HANDLER) t->unblock();
	}
}


Task & Scheduler::task(void * lx_task)
{
	for (Task * t = _present_list.first(); t; t = t->next()) {
		if (t->lx_task() == lx_task)
			return *t;
	}
	error("Lx_kit::Scheduler cannot find task ", lx_task);
	sleep_forever();
}


void Scheduler::schedule()
{
	/*
	 * Iterate over all tasks and run first runnable.
	 *
	 * (1) If one runnable tasks was run start over from beginning of
	 *     list.
	 *
	 * (2) If no task is runnable quit scheduling (break endless
	 *     loop).
	 */
	while (true) {
		bool at_least_one = false;

		/* update jiffies before running task */
		//Lx::timer_update_jiffies();

		for (Task * t = _present_list.first(); t; t = t->next()) {

			if (!t->runnable())
				continue;

			/* update current before running task */
			_current = t;
			t->run();
			at_least_one = true;

			if (!t->runnable())
				break;
		}

		if (!at_least_one)
			break;
	}

	/* clear current as no task is running */
	_current = nullptr;
}
