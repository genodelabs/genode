/*
 * \brief  Lx::Task represents a cooperatively scheduled thread of control
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/thread.h>
#include <base/sleep.h>
#include <lx_kit/task.h>
#include <lx_kit/scheduler.h>
#include <os/backtrace.h>

using namespace Lx_kit;

bool Task::runnable() const
{
	switch (_state) {
	case INIT:    return true;
	case RUNNING: return true;
	case BLOCKED: return false;
	}
	error("Invalid task state?!");
	return false;
}


static inline void * _alloc_stack(const char * name)
{
	enum { STACK_SIZE = 32 * 1024 };
	Genode::Thread * th = Genode::Thread::myself();
	return th->alloc_secondary_stack(name, STACK_SIZE);
}


Task::State Task::state() const { return _state; }


Task::Type Task::type() const { return _type; }


void * Task::lx_task() const { return _lx_task; }


int Task::pid() const { return _pid; }


int Task::priority() const { return _priority; }


void Task::priority(int prio)
{
	_scheduler.remove(*this);
	_priority = prio;
	_scheduler.add(*this);
}


void Task::name(const char * name) { _name = Task::Name(name); }


Task::Name Task::name() const { return _name; }


void Task::block()
{
	if (_state == RUNNING) _state = BLOCKED;
}


void Task::unblock()
{
	if (_state == BLOCKED) _state = RUNNING;
}


void Task::run()
{
	/*
	 * Save the execution environment. The scheduled task returns to this point
	 * after execution, i.e., at the next preemption point.
	 */
	if (_setjmp(_saved_env))
		return;

	if (_state == INIT) {
		/* setup execution environment and call task's function */
		_state = RUNNING;

		/* switch stack and call '_func(_arg)' */
		arch_execute(_stack, (void *)_func, _arg);
	} else {
		/* restore execution environment */
		_longjmp(_env, 1);
	}

	/* never reached */
	error("unexpected return of task");
	sleep_forever();
}


void Task::schedule()
{
	/*
	 * Save the execution environment. The task will resume from here on next
	 * schedule.
	 */
	if (_setjmp(_env))
		return;

	/* return to thread calling run() */
	_longjmp(_saved_env, 1);
}


void Task::block_and_schedule()
{
	block();
	schedule();
}


Task::Task(int       (* func)(void*),
           void       * arg,
           void       * lx_task,
           int          pid,
           char const * name,
           Scheduler  & scheduler,
           Type         type)
: _type(type),
  _scheduler(scheduler),
  _lx_task(lx_task),
  _pid(pid),
  _name(name),
  _stack(_alloc_stack(name)),
  _func(func),
  _arg(arg)
{
	_scheduler.add(*this);
}


Task::~Task() { _scheduler.remove(*this); }
