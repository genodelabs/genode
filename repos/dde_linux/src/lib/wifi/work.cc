/*
 * \brief  Workqueue implementation
 * \author Josef Soentgen
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/sleep.h>
#include <base/tslab.h>
#include <util/fifo.h>

/* local includes */
#include <lx.h>

#include <extern_c_begin.h>
# include <lx_emul.h>
#include <extern_c_end.h>


static void run_work(void *);
static void run_delayed_work(void *);

namespace Lx {
	class Work;
}

/**
 * Lx::Work
 */
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

	public:

		Work(void (*func)(void*), char const *name)
		:
			_task(func, nullptr, name, Lx::Task::PRIORITY_2, Lx::scheduler()),
			_work_alloc(Genode::env()->heap())
		{ }

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
};


static Lx::Work *_lx_work;
static Lx::Work *_lx_delayed_work;


void Lx::work_init(Server::Entrypoint &ep)
{
	static Lx::Work work_ctx(run_work, "work");
	_lx_work = &work_ctx;

	static Lx::Work delayed_work_ctx(run_delayed_work, "delayed_work");
	_lx_delayed_work = &delayed_work_ctx;
}


static void run_work(void *)
{
	while (1) {
		Lx::scheduler().current()->block_and_schedule();

		_lx_work->exec();
	}
}


static void run_delayed_work(void *)
{
	while (1) {
		Lx::scheduler().current()->block_and_schedule();

		_lx_delayed_work->exec();
	}
}


extern "C" {

/***********************
 ** linux/workquque.h **
 ***********************/

int schedule_delayed_work(struct delayed_work *work, unsigned long delay)
{
	_lx_delayed_work->schedule(work);
	_lx_delayed_work->unblock();

	return 0;
}


int schedule_work(struct work_struct *work)
{
	_lx_work->schedule(work);
	_lx_work->unblock();

	return 1;
}


bool cancel_delayed_work(struct delayed_work *dwork)
{
	int pending = timer_pending(&dwork->timer);
	del_timer(&dwork->timer);

	/* if the timer is still pending dwork was not executed */
	return pending;
}


bool cancel_delayed_work_sync(struct delayed_work *dwork)
{
	bool pending = cancel_delayed_work(dwork);

	if (pending) {
		PERR("WARN: delayed_work %p is executed directly in current '%s' routine",
		      dwork, Lx::scheduler().current()->name());

		dwork->work.func(&dwork->work);
	}

	return pending;
}


static void execute_delayed_work(unsigned long dwork)
{
	_lx_delayed_work->schedule((struct delayed_work *)dwork);
	_lx_delayed_work->unblock();
}


bool queue_delayed_work(struct workqueue_struct *wq,
                        struct delayed_work *dwork, unsigned long delay)
{
	/* treat delayed work without delay like any other work */
	if (delay == 0) {
		execute_delayed_work((unsigned long)dwork);
	} else {
		setup_timer(&dwork->timer, execute_delayed_work, (unsigned long)dwork);
		mod_timer(&dwork->timer, delay);
	}
	return true;
}


bool cancel_work_sync(struct work_struct *work)
{
	return _lx_work->cancel_work(work, true);
}


bool queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
	_lx_work->schedule(work);
	_lx_work->unblock();

	return true;
}


/***********************
 ** linux/interrupt.h **
 ***********************/

void tasklet_init(struct tasklet_struct *t, void (*f)(unsigned long), unsigned long d)
{
	t->func = f;
	t->data = d;
}


void tasklet_schedule(struct tasklet_struct *tasklet)
{
	_lx_work->schedule(tasklet);
}


void tasklet_hi_schedule(struct tasklet_struct *tasklet)
{
	tasklet_schedule(tasklet);
}

} /* extern "C" */
