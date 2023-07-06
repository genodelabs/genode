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

#include <base/entrypoint.h>
#include <util/fifo.h>
#include <util/list.h>
#include <lx_kit/task.h>
#include <lx_kit/pending_irq.h>

namespace Lx_kit {
	class Scheduler;
	class Task;

	using namespace Genode;
}


class Lx_kit::Scheduler
{
	private:

		Scheduler(Scheduler const &) = delete;
		Scheduler& operator=(const Scheduler&) = delete;

		List<Task> _present_list { };
		Task     * _current      { nullptr };
		Task     * _idle         { nullptr };

		Genode::Entrypoint &_ep;

		void _idle_pre_post_process();

		void _execute();

		Genode::Fifo<Lx_kit::Pending_irq> _pending_irqs { };

	public:

		Task & current();

		void idle(Task & idle) { _idle = &idle; }

		bool active() const;

		void add(Task & task);
		void remove(Task & task);

		void schedule();

		void execute();

		void unblock_irq_handler(Pending_irq &);
		template <typename FN> void pending_irq(FN const &fn)
		{
			_pending_irqs.dequeue([&] (Pending_irq const &pirq) {
				fn(pirq.value); });
		}

		void unblock_time_handler();

		Task & task(void * t);

		template <typename FN>
		void for_each_task(FN const & fn);

		Scheduler(Genode::Entrypoint &ep) : _ep { ep } { }
};


template <typename FN>
void Lx_kit::Scheduler::for_each_task(FN const & fn)
{
	for (Task * t = _present_list.first(); t; t = t->next())
		fn(*t);
}
