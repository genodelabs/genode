/*
 * \brief  Scheduler for executing Lx::Task objects
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

/* Genode includes */
#include <base/env.h>
#include <base/log.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <timer_session/connection.h>

/* Linux emulation environment includes */
#include <legacy/lx_kit/scheduler.h>

#include <legacy/lx_kit/timer.h>


namespace Lx_kit {
	class Scheduler;
}


class Lx_kit::Scheduler : public Lx::Scheduler
{
	private:

		bool verbose = false;

		Lx_kit::List<Lx::Task> _present_list;

		Lx::Task *_current = nullptr; /* currently scheduled task */

		bool _run_task(Lx::Task *);

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

		struct Logger : Genode::Thread
		{
			Timer::Connection       _timer;
			Lx::Scheduler          &_scheduler;
			Genode::uint64_t const  _interval;

			Logger(Genode::Env &env, Lx::Scheduler &scheduler,
			       Genode::uint64_t interval_seconds)
			:
				Genode::Thread(env, "logger", 0x4000),
				_timer(env), _scheduler(scheduler),
				_interval(interval_seconds)
			{
				start();
			}

			void entry()
			{
				_timer.msleep(1000 * _interval);
				while (true) {
					_scheduler.log_state("LOGGER");
					_timer.msleep(2000);
				}
			}
		};

		Genode::Constructible<Logger> _logger;

	public:

		Scheduler(Genode::Env &env)
		{
			if (verbose) { _logger.construct(env, *this, 10); }
		}

		/*****************************
		 ** Lx::Scheduler interface **
		 *****************************/

		Lx::Task *current() override
		{
			if (!_current) {
				Genode::error("BUG: _current is zero!");
				Genode::sleep_forever();
			}

			return _current;
		}

		bool active() const override {
			return _current != nullptr; }

		void add(Lx::Task *task) override
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

		void remove(Lx::Task *task) override
		{
			_present_list.remove(task);
		}

		void schedule() override
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
				for (Lx::Task *t = _present_list.first(); t; t = t->next()) {
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
				Genode::warning("schedule() called without runnable tasks");
				log_state("SCHEDULE");
			}

			/* clear current as no task is running */
			_current = nullptr;
		}

		void log_state(char const *prefix) override
		{
			unsigned  i;
			Lx::Task *t;
			for (i = 0, t = _present_list.first(); t; t = t->next(), ++i) {
				Genode::log(prefix, " [", i, "] "
				            "prio: ", (int)t->priority(), " "
				            "state: ", _state_color(t->state()), (int)t->state(),
				                       _ansi_esc_reset(), " ",
				            t->name());
			}
		}
};


/*****************************
 ** Lx::Task implementation **
 *****************************/

Lx::Task::Task(void (*func)(void*), void *arg, char const *name,
               Priority priority, Scheduler &scheduler)
:
	_priority(priority), _scheduler(scheduler),
	_func(func), _arg(arg), _name(name)
{
	scheduler.add(this);

	if (verbose)
		Genode::log("name: '", name, "' " "func: ", func, " "
		            "arg: ",   arg, " prio: ", (int)priority, " t: ", this);
}


Lx::Task::~Task()
{
	_scheduler.remove(this);

	if (_stack)
		Genode::Thread::myself()->free_secondary_stack(_stack);
}


/**********************************
 ** Lx::Scheduler implementation **
 **********************************/

Lx::Scheduler &Lx::scheduler(Genode::Env *env)
{
	static Lx_kit::Scheduler inst { *env };
	return inst;
}
