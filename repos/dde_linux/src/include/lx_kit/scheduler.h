/*
 * \brief  Scheduler for executing Task objects
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

#include <util/list.h>
#include <lx_kit/task.h>

namespace Lx_kit {
	class Scheduler;
	class Task;

	using namespace Genode;
}


class Lx_kit::Scheduler
{
	private:

		List<Task> _present_list { };
		Task     * _current      { nullptr };

	public:

		Task & current();

		bool active() const;

		void add(Task & task);
		void remove(Task & task);

		void schedule();
		void unblock_irq_handler();
		void unblock_time_handler();

		Task & task(void * t);

		template <typename FN>
		void for_each_task(FN const & fn);
};


template <typename FN>
void Lx_kit::Scheduler::for_each_task(FN const & fn)
{
	for (Task * t = _present_list.first(); t; t = t->next())
		fn(*t);
}
