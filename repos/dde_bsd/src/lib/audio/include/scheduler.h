/*
 * \brief  User-level scheduling
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Christian Helmuth
 * \date   2012-04-25
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

/* Genode includes */
#include <base/env.h>
#include <base/lock.h>
#include <base/sleep.h>

/* local includes */
#include <list.h>
#include <platform/platform.h>


namespace Bsd {
	class Scheduler;
	class Task;

	Scheduler &scheduler();
};


/**
 * Allows pseudo-parallel execution of functions
 */
class Bsd::Task : public Bsd::List<Bsd::Task>::Element
{
	public:

		enum Priority { PRIORITY_0, PRIORITY_1, PRIORITY_2, PRIORITY_3 };

		/**
		 * Runtime state
		 *
		 *                                INIT
		 *                                 |
		 *                               [run]
		 *                                 v
		 * BLOCKED <--[block/unblock]--> RUNNING
		 */
		enum State { STATE_INIT, STATE_RUNNING, STATE_BLOCKED };

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
		int const    _stack_size;        /* size of the stack of task */

	public:

		Task(void (*func)(void*), void *arg, char const *name,
		     Priority priority, Scheduler &scheduler, int stack_size);
		virtual ~Task();

		State    state()    const { return _state; }
		Priority priority() const { return _priority; }


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
 */
class Bsd::Scheduler
{
	private:

		Bsd::List<Bsd::Task> _present_list;
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
};

#endif /* _SCHEDULER_H_ */
