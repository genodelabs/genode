/*
 * \brief  Work queue implementation
 * \author Josef Soentgen
 * \author Stefan Kalkowski
 * \date   2015-10-26
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/tslab.h>

#include <lx_kit/internal/list.h>
#include <lx_kit/scheduler.h>

/* Linux emulation environment includes */
#include <lx_emul.h>

#include <lx_kit/work.h>


namespace Lx_kit { class Work; }


class Lx_kit::Work : public Lx::Work
{
	public:

		/**
		 * Context encapsulates a normal work item
		 */
		struct Context : public Lx_kit::List<Context>::Element
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

		Lx::Task              _task;
		Lx_kit::List<Context> _list;

		Genode::Tslab<Context, 64 * sizeof(Context)> _work_alloc;

		/**
		 * Schedule work item
		 */
		template <typename WORK>
		void _schedule(WORK *work)
		{
			Context *c = new (&_work_alloc) Context(work);
			_list.append(c);
		}

	public:

		Work(Genode::Allocator &alloc, char const *name = "work_queue")
		: _task(Work::run_work, reinterpret_cast<void*>(this), name,
		        Lx::Task::PRIORITY_2, Lx::scheduler()),
		  _work_alloc(&alloc) { }

		/**
		 * Execute all available work items
		 */
		void exec()
		{
			while (Context *c = _list.first()) {
				_list.remove(c);
				c->exec();
				destroy(&_work_alloc, c);
			}
		}

		static void run_work(void *wq)
		{
			Work *work_queue = reinterpret_cast<Work*>(wq);
			while (1) {
				work_queue->exec();
				Lx::scheduler().current()->block_and_schedule();
			}
		}

		/************************
		 ** Lx::Work interface **
		 ************************/

		void unblock() { _task.unblock(); }

		void schedule(struct work_struct *work) {
			_schedule(work); }

		void schedule_delayed(struct delayed_work *work,
		                      unsigned long /*delay*/) {
			_schedule(work); }

		void schedule_tasklet(struct tasklet_struct *tasklet) {
			_schedule(tasklet); }

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

		char const *task_name() override { return _task.name(); }
};


/*****************************
 ** Lx::Work implementation **
 *****************************/

Lx::Work & Lx::Work::work_queue(Genode::Allocator *alloc)
{
	static Lx_kit::Work inst(*alloc);
	return inst;
}


Lx::Work * Lx::Work::alloc_work_queue(Genode::Allocator *alloc, char const *name)
{
	Lx::Work *work = new (alloc) Lx_kit::Work(*alloc, name);
	return work;
}


void Lx::Work::free_work_queue(Lx::Work *w)
{
	Genode::error(__func__, ": IMPLEMENT ME");
}
