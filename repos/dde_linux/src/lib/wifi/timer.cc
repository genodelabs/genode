/*
 * \brief  Signal context for timer events
 * \author Josef Soentgen
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/heap.h>
#include <base/printf.h>
#include <base/tslab.h>
#include <timer_session/connection.h>

/* local includes */
#include <list.h>
#include <lx.h>

#include <extern_c_begin.h>
# include <lx_emul.h>
#include <extern_c_end.h>


unsigned long jiffies;


static void run_timer(void*);

namespace Lx {
	class Timer;
}

/**
 * Lx::Timer
 */
class Lx::Timer
{
	public:

		/**
		 * Context encapsulates a regular linux timer_list
		 */
		struct Context : public Lx::List<Context>::Element
		{
			enum { INVALID_TIMEOUT = ~0UL };

			struct timer_list *timer;
			bool               pending { false };
			unsigned long      timeout { INVALID_TIMEOUT }; /* absolute in jiffies */
			bool               programmed { false };

			Context(struct timer_list *timer) : timer(timer) { }
		};

	private:

		::Timer::Connection                          _timer_conn;
		Lx::List<Context>                            _list;
		Lx::Task                                     _timer_task;
		Genode::Signal_rpc_member<Lx::Timer>         _dispatcher;
		Genode::Tslab<Context, 32 * sizeof(Context)> _timer_alloc;

		/**
		 * Lookup local timer
		 */
		Context *_find_context(struct timer_list const *timer)
		{
			for (Context *c = _list.first(); c; c = c->next())
				if (c->timer == timer)
					return c;

			return 0;
		}

		/**
		 * Program the first timer in the list
		 *
		 * The first timer is programmed if the 'programmed' flag was not set
		 * before. The second timer is flagged as not programmed as
		 * 'Timer::trigger_once' invalidates former registered one-shot
		 * timeouts.
		 */
		void _program_first_timer()
		{
			Context *ctx = _list.first();
			if (!ctx)
				return;

			if (ctx->programmed)
				return;

			/* calculate relative microseconds for trigger */
			unsigned long us = ctx->timeout > jiffies ?
			                   jiffies_to_msecs(ctx->timeout - jiffies) * 1000 : 0;
			_timer_conn.trigger_once(us);

			ctx->programmed = true;

			/* possibly programmed successor must be reprogrammed later */
			if (Context *next = ctx->next())
				next->programmed = false;
		}

		/**
		 * Schedule timer
		 *
		 * Add the context to the scheduling list depending on its timeout
		 * and reprogram the first timer.
		 */
		void _schedule_timer(Context *ctx, unsigned long expires)
		{
			_list.remove(ctx);

			ctx->timeout    = expires;
			ctx->pending    = true;
			ctx->programmed = false;
			/*
			 * Also write the timeout value to the expires field in
			 * struct timer_list because the wireless stack checks
			 * it directly.
			 */
			ctx->timer->expires = expires;

			Context *c;
			for (c = _list.first(); c; c = c->next())
				if (ctx->timeout <= c->timeout)
					break;
			_list.insert_before(ctx, c);

			_program_first_timer();
		}

		/**
		 * Handle trigger_once signal
		 */
		void _handle(unsigned)
		{
			_timer_task.unblock();

			Lx::scheduler().schedule();
		}

	public:

		/**
		 * Constructor
		 */
		Timer(Server::Entrypoint &ep)
		:
			_timer_task(run_timer, nullptr, "timer", Lx::Task::PRIORITY_2,
			            Lx::scheduler()),
			_dispatcher(ep, *this, &Lx::Timer::_handle),
			_timer_alloc(Genode::env()->heap())
		{
			_timer_conn.sigh(_dispatcher);
		}

		/**
		 * Add new linux timer
		 */
		void add(struct timer_list *timer)
		{
			Context *t = new (&_timer_alloc) Context(timer);
			_list.append(t);
		}

		/**
		 * Delete linux timer
		 */
		int del(struct timer_list *timer)
		{
			Context *ctx = _find_context(timer);

			/**
			 * If the timer expired it was already cleaned up after its
			 * execution.
			 */
			if (!ctx)
				return 0;

			int rv = ctx->timeout != Context::INVALID_TIMEOUT ? 1 : 0;

			_list.remove(ctx);
			destroy(&_timer_alloc, ctx);

			return rv;
		}

		/**
		 * Initial scheduling of linux timer
		 */
		int schedule(struct timer_list *timer, unsigned long expires)
		{
			Context *ctx = _find_context(timer);
			if (!ctx) {
				PERR("schedule unknown timer %p", timer);
				return -1; /* XXX better use 0 as rv? */
			}

			/*
			 * If timer was already active return 1, otherwise 0. The return
			 * value is needed by mod_timer().
			 */
			int rv = ctx->timeout != Context::INVALID_TIMEOUT ? 1 : 0;

			_schedule_timer(ctx, expires);

			return rv;
		}

		/**
		 * Schedule next linux timer
		 */
		void schedule_next() { _program_first_timer(); }

		/**
		 * Check if the timer is currently pending
		 */
		bool pending(struct timer_list const *timer)
		{
			Context *ctx = _find_context(timer);
			if (!ctx) {
				return false;
			}

			return ctx->pending;
		}

		Context *find(struct timer_list const *timer) {
			return _find_context(timer); }

		/**
		 * Update jiffie counter
		 */
		void update_jiffies()
		{
			jiffies = msecs_to_jiffies(_timer_conn.elapsed_ms());
		}

		/**
		 * Get first timer context
		 */
		Context* first() { return _list.first(); }
};


static Lx::Timer *_lx_timer;


void Lx::timer_init(Server::Entrypoint &ep)
{
	/* XXX safer way preventing possible nullptr access? */
	static Lx::Timer lx_timer(ep);
	_lx_timer = &lx_timer;

	/* initialize value explicitly */
	jiffies = 0UL;
}


void Lx::timer_update_jiffies() {
	_lx_timer->update_jiffies(); }


static void run_timer(void *)
{
	while (1) {
		Lx::scheduler().current()->block_and_schedule();

		while (Lx::Timer::Context *ctx = _lx_timer->first()) {
			if (ctx->timeout > jiffies)
				break;

			ctx->timer->function(ctx->timer->data);
			_lx_timer->del(ctx->timer);
		}

		_lx_timer->schedule_next();
	}
}


/*******************
 ** linux/timer.h **
 *******************/

void init_timer(struct timer_list *timer) { }


int mod_timer(struct timer_list *timer, unsigned long expires)
{
	if (!_lx_timer->find(timer))
		_lx_timer->add(timer);

	return _lx_timer->schedule(timer, expires);
}


void setup_timer(struct timer_list *timer,void (*function)(unsigned long),
                 unsigned long data)
{
	timer->function = function;
	timer->data     = data;
	init_timer(timer);
}


int timer_pending(const struct timer_list *timer)
{
	bool pending = _lx_timer->pending(timer);

	return pending;
}


int del_timer(struct timer_list *timer)
{
	int rv = _lx_timer->del(timer);
	_lx_timer->schedule_next();

	return rv;
}


/*******************
 ** linux/sched.h **
 *******************/

static void unblock_task(unsigned long task)
{
	Lx::Task *t = (Lx::Task *)task;

	t->unblock();
}


signed long schedule_timeout(signed long timeout)
{
	struct timer_list timer;

	setup_timer(&timer, unblock_task, (unsigned long)Lx::scheduler().current());
	mod_timer(&timer, timeout);

	Lx::scheduler().current()->block_and_schedule();

	del_timer(&timer);

	timeout = (timeout - jiffies);

	return timeout < 0 ? 0 : timeout;
}
