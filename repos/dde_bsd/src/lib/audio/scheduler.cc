/*
 * \brief  User-level scheduling
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Christian Helmuth
 * \date   2012-04-25
 *
 * We use a pseudo-thread implementation based on setjmp/longjmp.
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/sleep.h>

/* local includes */
#include <bsd.h>
#include <scheduler.h>


static bool const debugging = false;
static bool const verbose   = false;
#define PDBGV(...)  do { if (verbose) PDBG(__VA_ARGS__); } while (0)


/**********
 ** Task **
 **********/

bool Bsd::Task::_runnable() const
{
	switch (_state) {
	case STATE_INIT:          return true;
	case STATE_RUNNING:       return true;
	case STATE_BLOCKED:       return false;
	}

	PERR("state %d not handled by switch", _state);
	Genode::sleep_forever();
}


bool Bsd::Task::run()
{
	if (!_runnable())
		return false;

	/*
	 * Save the execution environment. The scheduled task returns to this point
	 * after execution, i.e., at the next preemption point.
	 */
	if (_setjmp(_saved_env))
		return true;

	if (_state == STATE_INIT) {
		/* setup execution environment and call task's function */
		_state = STATE_RUNNING;
		Genode::Thread_base *th = Genode::Thread_base::myself();

		_stack = th->alloc_secondary_stack(_name, _stack_size);

		/* switch stack and call '_func(_arg)' */
		platform_execute(_stack, (void *)_func, _arg);
	} else {
		/* restore execution environment */
		_longjmp(_env, 1);
	}

	/* never reached */
	PERR("Unexpected return of Task");
	Genode::sleep_forever();
}


void Bsd::Task::schedule()
{
	/*
	 * Save the execution environment. The task will resume from here on next
	 * schedule.
	 */
	if (_setjmp(_env)) {
		return;
	}

	/* return to thread calling run() */
	_longjmp(_saved_env, 1);
}


Bsd::Task::Task(void (*func)(void*), void *arg, char const *name,
               Priority priority, Scheduler &scheduler, int stack_size)
:
	_priority(priority), _scheduler(scheduler),
	_func(func), _arg(arg), _name(name), _stack_size(stack_size)
{
	scheduler.add(this);

	PDBGV("name: '%s' func: %p arg: %p prio: %u t: %p", name, func, arg, priority, this);
}


Bsd::Task::~Task()
{
	if (_stack)
		Genode::Thread_base::myself()->free_secondary_stack(_stack);
}


/***************
 ** Scheduler **
 ***************/

Bsd::Scheduler & Bsd::scheduler()
{
	static Bsd::Scheduler inst;
	return inst;
}


Bsd::Task *Bsd::Scheduler::current()
{
	if (!_current) {
		PERR("BUG: _current is zero!");
		Genode::sleep_forever();
	}

	return _current;
}


void Bsd::Scheduler::add(Task *task)
{
	Bsd::Task *p = _present_list.first();
	for ( ; p; p = p->next()) {
		if (p->priority() <= task->priority()) {
			_present_list.insert_before(task, p);
			break;
		}
	}
	if (!p)
		_present_list.append(task);
}


void Bsd::Scheduler::schedule()
{
	bool at_least_one = false;

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
		/* update current time before running task */
		Bsd::update_time();

		bool was_run = false;
		for (Task *t = _present_list.first(); t; t = t->next()) {
			/* update current before running task */
			_current = t;

			if ((was_run = t->run())) {
				at_least_one = true;
				break;
			}
		}
		if (!was_run)
			break;
	}

	if (!at_least_one) {
		PWRN("schedule() called without runnable tasks");
		log_state("SCHEDULE");
	}

	/* clear current as no task is running */
	_current = nullptr;
}


#include <timer_session/connection.h>

namespace {
	struct Logger : Genode::Thread<0x4000>
	{
		Timer::Connection  _timer;
		Bsd::Scheduler     &_scheduler;
		unsigned     const _interval;

		Logger(Bsd::Scheduler &scheduler, unsigned interval_seconds)
		:
			Genode::Thread<0x4000>("logger"),
			_scheduler(scheduler), _interval(interval_seconds)
		{
			start();
		}

		void entry()
		{
			PWRN("Scheduler::Logger is up");
			_timer.msleep(1000 * _interval);
			while (true) {
				_scheduler.log_state("LOGGER");
				_timer.msleep(2000);
			}
		}
	};
}

#define ANSI_ESC_RESET      "\033[00m"
#define ANSI_ESC_BLACK      "\033[30m"
#define ANSI_ESC_RED        "\033[31m"
#define ANSI_ESC_YELLOW     "\033[33m"

static char const *state_color(Bsd::Task::State state)
{
	switch (state) {
	case Bsd::Task::STATE_INIT:          return ANSI_ESC_RESET;
	case Bsd::Task::STATE_RUNNING:       return ANSI_ESC_RED;
	case Bsd::Task::STATE_BLOCKED:       return ANSI_ESC_YELLOW;
	}

	return ANSI_ESC_BLACK;
}


void Bsd::Scheduler::log_state(char const *prefix)
{
	unsigned  i;
	Bsd::Task *t;
	for (i = 0, t = _present_list.first(); t; t = t->next(), ++i) {
		Genode::printf("%s [%u] prio: %u state: %s%u" ANSI_ESC_RESET " %s\n",
		               prefix, i, t->priority(), state_color(t->state()),
		               t->state(), t->name());
	}
}


Bsd::Scheduler::Scheduler()
{
	if (debugging)
		new (Genode::env()->heap()) Logger(*this, 10);
}


Bsd::Scheduler::~Scheduler() { }
