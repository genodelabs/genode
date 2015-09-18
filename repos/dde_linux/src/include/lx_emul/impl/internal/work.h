/*
 * \brief  Work queue implementation
 * \author Josef Soentgen
 * \author Stefan Kalkowski
 * \date   2015-10-26
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_EMUL__IMPL__INTERNAL__WORK_H_
#define _LX_EMUL__IMPL__INTERNAL__WORK_H_

/* Linux emulation environment includes */
#include <lx_emul/impl/internal/task.h>
#include <lx_emul/impl/internal/list.h>

namespace Lx {
	class Work;
}


class Lx::Work
{
	public:

		/**
		 * Context encapsulates a normal work item
		 */
		struct Context : public Lx::List<Context>::Element
		{
			void *work;
			enum Type { NORMAL, DELAYED, TASKLET } type;

			void exec() {
				switch (type) {
				case NORMAL:
					{
						work_struct *w = static_cast<work_struct *>(work);
						w->func(w);
					}
					break;
				case DELAYED:
					{
						delayed_work *w = static_cast<delayed_work *>(work);
						w->work.func(&(w)->work);
					}
					break;
				case TASKLET:
					{
						tasklet_struct *tasklet = static_cast<tasklet_struct *>(work);
						tasklet->func(tasklet->data);
					}
					break;
				}
			}

			Context(delayed_work *w)   : work(w), type(DELAYED) { }
			Context(work_struct  *w)   : work(w), type(NORMAL)  { }
			Context(tasklet_struct *w) : work(w), type(TASKLET) { }
		};

	private:

		Lx::Task           _task;
		Lx::List<Context>  _list;

		Genode::Tslab<Context, 64 * sizeof(Context)> _work_alloc;

		Work()
		: _task(Work::run_work, reinterpret_cast<void*>(this), "work_queue",
		        Lx::Task::PRIORITY_2, Lx::scheduler()),
		  _work_alloc(Genode::env()->heap()) { }

	public:

		static Work & work_queue();

		/**
		 * Unblock corresponding task
		 */
		void unblock() { _task.unblock(); }

		/**
		 * Schedule work item
		 */
		template <typename WORK>
		void schedule(WORK *work)
		{
			Context *c = new (&_work_alloc) Context(work);
			_list.append(c);
		}

		/**
		 * Execute all available work items
		 */
		void exec()
		{
			while (Context *c = _list.first()) {
				c->exec();
				_list.remove(c);
				destroy(&_work_alloc, c);
			}
		}

		/**
		 * Cancel work item
		 */
		bool cancel_work(struct work_struct *work, bool sync = false)
		{
			for (Context *c = _list.first(); c; c = c->next()) {
				if (c->work == work) {
					if (sync)
						c->exec();

					_list.remove(c);
					destroy(&_work_alloc, c);
					return true;
				}
			}

			return false;
		}

		static void run_work(void * wq)
		{
			Work * work_queue = reinterpret_cast<Work*>(wq);
			while (1) {
				work_queue->exec();
				Lx::scheduler().current()->block_and_schedule();
			}
		}
};


Lx::Work & Lx::Work::work_queue()
{
	static Lx::Work work;
	return work;
}

#endif /* _LX_EMUL__IMPL__INTERNAL__WORK_H_ */
