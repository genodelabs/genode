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

#ifndef _LX_KIT__SCHEDULER_H_
#define _LX_KIT__SCHEDULER_H_

/* Genode includes */
#include <base/env.h>

namespace Lx {

	class Task;
	class Scheduler;

	/**
	 * Return singleton instance of the scheduler
	 *
	 * Implementation must be provided by the driver.
	 */
	Scheduler &scheduler(Genode::Env *env = nullptr);
}


class Lx::Scheduler
{
	public:

		/**
		 * Return currently scheduled task
		 */
		virtual Task *current() = 0;

		/**
		 * Return true if a task is currently running
		 */
		virtual bool active() const = 0;

		/**
		 * Add new task to the present list
		 */
		virtual void add(Task *task) = 0;

		/**
		 * Remove a task
		 */
		virtual void remove(Task *task) = 0;

		/**
		 * Schedule all present tasks
		 *
		 * Returns if no task is runnable.
		 */
		virtual void schedule() = 0;

		/**
		 * Log current state of tasks in present list (debug)
		 *
		 * Log lines are prefixed with 'prefix'.
		 */
		virtual void log_state(char const *prefix) = 0;
};

#include <legacy/lx_kit/internal/task.h>


#endif /* _LX_KIT__SCHEDULER_H_ */
