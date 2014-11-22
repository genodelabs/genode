/*
 * \brief  User-level scheduling
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Christian Helmuth
 * \date   2012-04-25
 */

/*
 * Copyright (C) 2012-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

/* Genode includes */
#include <base/env.h>
#include <base/lock.h>
#include <base/sleep.h>

/* libc includes */
#include <setjmp.h>

/* local includes */
#include <list.h>


namespace Lx {
	class Scheduler;
	class Task;

	Scheduler &scheduler();
};


/**
 * Allows pseudo-parallel execution of functions
 */
class Lx::Task : public Lx::List<Lx::Task>::Element
{
	public:

		/**
		 * TODO generalize - higher is more important
		 */
		enum Priority { PRIORITY_0, PRIORITY_1, PRIORITY_2, PRIORITY_3 };

		/**
		 * Runtime state
		 *
		 *                        INIT
		 *                         |
		 *                       [run]
		 *                         v
		 * BLOCKED <--[block]--- RUNNING ---[mutex_block]--> MUTEX_BLOCKED
		 *         --[unblock]->         <-[mutex_unblock]--
		 *
		 * Transitions between BLOCKED and MUTEX_BLOCKED are not possible.
		 */
		enum State { STATE_INIT, STATE_RUNNING, STATE_BLOCKED, STATE_MUTEX_BLOCKED, STATE_WAIT_BLOCKED };

		/**
		 * List element type
		 */
		typedef Lx::List_element<Lx::Task> List_element;

		/**
		 * List type
		 */
		typedef Lx::List<List_element> List;

	private:

		State _state = STATE_INIT;

		/* sub-classes may overwrite the runnable condition */
		virtual bool _runnable() const;

		void        *_stack     = nullptr;       /* stack pointer */
		jmp_buf      _env;                       /* execution state */
		jmp_buf      _saved_env;                 /* saved state of thread calling run() */

		Priority     _priority;
		Scheduler   &_scheduler;         /* scheduler this task is attached to */

		void       (*_func)(void *);     /* function to call*/
		void        *_arg;               /* argument for function */
		char const  *_name;              /* name of task */

		List_element _mutex_le { this }; /* list element for mutex_blocked state */

		List         *_wait_list { 0 };
		List_element  _wait_le { this };
		bool          _wait_le_enqueued { false };

	public:

		Task(void (*func)(void*), void *arg, char const *name,
		     Priority priority, Scheduler &scheduler);
		virtual ~Task();

		State    state()    const { return _state; }
		Priority priority() const { return _priority; }

		void wait_enqueue(List *list)
		{
			if (_wait_le_enqueued) {
				PERR("%p already queued in %p", this, _wait_list);
				Genode::sleep_forever();
			}

			_wait_le_enqueued = true;
			_wait_list = list;
			_wait_list->append(&_wait_le);
		}

		void wait_dequeue(List *list)
		{
			if (!_wait_le_enqueued) {
				PERR("%p not queued", this);
				Genode::sleep_forever();
			}
			
			if (_wait_list != list) {
				PERR("especially not in list %p", list);
				Genode::sleep_forever();
			}

			_wait_list->remove(&_wait_le);
			_wait_list = 0;
			_wait_le_enqueued = false;
		}

		/*******************************
		 ** Runtime state transitions **
		 *******************************/

		void block()
		{
			if (_state == STATE_RUNNING) {
				_state = STATE_BLOCKED;
			}
		}

		void unblock()
		{
			if (_state == STATE_BLOCKED) {
				_state = STATE_RUNNING;
			}
		}

		void mutex_block(List *list)
		{
			if (_state == STATE_RUNNING) {
				_state = STATE_MUTEX_BLOCKED;
				list->append(&_mutex_le);
			}
		}

		void mutex_unblock(List *list)
		{
			if (_state == STATE_MUTEX_BLOCKED) {
				_state = STATE_RUNNING;
				list->remove(&_mutex_le);
			}
		}

		/**
		 * Run task until next preemption point
		 *
		 * \return true if run, false if not runnable
		 */
		bool run();

		/**
		 * Request scheduling (of other tasks)
		 *
		 * Note, this task may not be blocked when calling schedule() depending
		 * on the use case.
		 */
		void schedule();

		/**
		 * Shortcut to enter blocking state and request scheduling
		 */
		void block_and_schedule()
		{
			block();
			schedule();
		}

		/**
		 * Return the name of the task (mainly for debugging purposes)
		 */
		char const *name() { return _name; }
};


/**
 * Scheduler
 *
 * FIXME The old mechanism for removal via check_dead() was removed and has to
 *       be reimplemented later.
 */
class Lx::Scheduler
{
	private:

		Lx::List<Lx::Task> _present_list;
		Genode::Lock       _present_list_mutex;

		Task *_current = nullptr; /* currently scheduled task */

		bool _run_task(Task *);

	public:

		Scheduler();
		~Scheduler();

		/**
		 * Return currently scheduled task
		 */
		Task *current();

		/**
		 * Add new task to the present list
		 */
		void add(Task *);

		/**
		 * Schedule all present tasks
		 *
		 * Returns if no task is runnable.
		 */
		void schedule();

		/**
		 * Log current state of tasks in present list (debug)
		 *
		 * Log lines are prefixed with 'prefix'
		 */
		void log_state(char const *prefix);
};

#endif /* _SCHEDULER_H_ */
