/*
 * \brief  Scheduler for executing Lx::Task objects
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_EMUL__IMPL__INTERNAL__SCHEDULER_H_
#define _LX_EMUL__IMPL__INTERNAL__SCHEDULER_H_

/* Genode includes */
#include <base/env.h>
#include <base/lock.h>
#include <base/printf.h>
#include <base/thread.h>
#include <timer_session/connection.h>

/* Linux emulation environment includes */
#include <lx_emul/impl/internal/task.h>
#include <lx_emul/impl/internal/debug.h>

namespace Lx {

	class Scheduler;

	/**
	 * Return singleton instance of the scheduler
	 *
	 * Implementation must be provided by the driver.
	 */
	Scheduler &scheduler();

	/**
	 * Called each time when a scheduling decision is taken
	 *
	 * Must be provided by the compilation unit that includes 'scheduler.h',
	 * e.g., by also including 'timer.h'.
	 */
	static inline void timer_update_jiffies();
}


class Lx::Scheduler
{
	private:

		bool verbose = false;

		Lx::List<Lx::Task> _present_list;
		Genode::Lock       _present_list_mutex;

		Task *_current = nullptr; /* currently scheduled task */

		bool _run_task(Task *);

		/*
		 * Support for logging
		 */

		static inline char const *_ansi_esc_reset()  { return "\033[00m"; }
		static inline char const *_ansi_esc_black()  { return "\033[30m"; }
		static inline char const *_ansi_esc_red()    { return "\033[31m"; }
		static inline char const *_ansi_esc_yellow() { return "\033[33m"; }

		static inline char const *_state_color(Lx::Task::State state)
		{
			switch (state) {
			case Lx::Task::STATE_INIT:          return _ansi_esc_reset();
			case Lx::Task::STATE_RUNNING:       return _ansi_esc_red();
			case Lx::Task::STATE_BLOCKED:       return _ansi_esc_yellow();
			case Lx::Task::STATE_MUTEX_BLOCKED: return _ansi_esc_yellow();
			case Lx::Task::STATE_WAIT_BLOCKED:  return _ansi_esc_yellow();
			}

			return _ansi_esc_black();
		}

		struct Logger : Genode::Thread<0x4000>
		{
			Timer::Connection  _timer;
			Lx::Scheduler     &_scheduler;
			unsigned     const _interval;

			Logger(Lx::Scheduler &scheduler, unsigned interval_seconds)
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

	public:

		Scheduler()
		{
			if (verbose)
				new (Genode::env()->heap()) Logger(*this, 10);
		}

		/**
		 * Return currently scheduled task
		 */
		Task *current()
		{
			if (!_current) {
				PERR("BUG: _current is zero!");
				Genode::sleep_forever();
			}

			return _current;
		}

		/**
		 * Add new task to the present list
		 */
		void add(Task *task)
		{
			Lx::Task *p = _present_list.first();
			for ( ; p; p = p->next()) {
				if (p->priority() <= task->priority()) {
					_present_list.insert_before(task, p);
					break;
				}
			}
			if (!p)
				_present_list.append(task);
		}

		/**
		 * Schedule all present tasks
		 *
		 * Returns if no task is runnable.
		 */
		void schedule()
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
				/* update jiffies before running task */
				Lx::timer_update_jiffies();

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

		/**
		 * Log current state of tasks in present list (debug)
		 *
		 * Log lines are prefixed with 'prefix'.
		 */
		void log_state(char const *prefix)
		{
			unsigned  i;
			Lx::Task *t;
			for (i = 0, t = _present_list.first(); t; t = t->next(), ++i) {
				Genode::printf("%s [%u] prio: %u state: %s%u%s %s\n",
				               prefix, i, t->priority(), _state_color(t->state()),
				               t->state(), _ansi_esc_reset(), t->name());
			}
		}
};


Lx::Task::Task(void (*func)(void*), void *arg, char const *name,
               Priority priority, Scheduler &scheduler)
:
	_priority(priority), _scheduler(scheduler),
	_func(func), _arg(arg), _name(name)
{
	scheduler.add(this);

	PDBGV("name: '%s' func: %p arg: %p prio: %u t: %p", name, func, arg, priority, this);
}


void Lx::Task::_deinit()
{
	if (_stack)
		Genode::Thread_base::myself()->free_secondary_stack(_stack);
}


#endif /* _LX_EMUL__IMPL__INTERNAL__SCHEDULER_H_ */
