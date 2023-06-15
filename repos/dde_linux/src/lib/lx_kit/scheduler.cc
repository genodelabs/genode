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
#include <base/entrypoint.h>
#include <base/log.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <os/backtrace.h>

#include <lx_kit/env.h>
#include <lx_kit/task.h>

using namespace Genode;
using namespace Lx_kit;

void Scheduler::_idle_pre_post_process()
{
	if (!_idle)
		return;

	_current = _idle;
	_idle->run();
}


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


void Scheduler::execute()
{
	/* sanity check that right thread & stack is in use */
	auto const thread = Genode::Thread::myself();
	if (!_ep.rpc_ep().myself(addr_t(&thread))) {
		Genode::error("Lx_kit::Scheduler called by invalid thread/stack ",
		              thread->name(), " ",
		              Genode::Hex(thread->mystack().base), "-",
		              Genode::Hex(thread->mystack().top));
		Genode::backtrace();
		Genode::sleep_forever();
	}

	_schedule();
}


/*
 * This signal handler function must only be called from within an EP
 * context, see check in 'execute()'.
 */
void Scheduler::_schedule()
{
	_idle_pre_post_process();

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

		for (Task * t = _present_list.first(); t; ) {

			Task *tmp = t;
			t = t->next();

			if (!tmp->destroy())
				continue;

			Genode::destroy(Lx_kit::env().heap, tmp);
		}

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

	_idle_pre_post_process();

	/* clear current as no task is running */
	_current = nullptr;
}
