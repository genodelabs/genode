/*
 * \brief  Timer
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_EMUL__IMPL__INTERNAL__TIMER_H_
#define _LX_EMUL__IMPL__INTERNAL__TIMER_H_

/* Genode includes */
#include <os/server.h>
#include <base/tslab.h>
#include <timer_session/connection.h>

/* Linux emulation environment includes */
#include <lx_emul/impl/internal/task.h>
#include <lx_emul/impl/internal/scheduler.h>

namespace Lx {

	class Timer;

	/**
	 * Return singleton instance of timer
	 *
	 * \param ep          entrypoint used handle timeout signals,
	 *                    to be specified at the first call of the function,
	 *                    which implicitly initializes the timer
	 * \param jiffies_ptr pointer to jiffies counter to be periodically
	 *                    updated
	 */
	Timer &timer(Server::Entrypoint *ep = nullptr,
	             unsigned long *jiffies_ptr = nullptr);

	/**
	 * Blue-print implementation of 'timer' function
	 */
	static inline Timer &_timer_impl(Server::Entrypoint *ep,
	                                 unsigned long *jiffies_ptr);

	static inline void timer_update_jiffies();

	static inline void run_timer(void *);
}


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


		unsigned long                               &_jiffies;
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
			unsigned long us = ctx->timeout > _jiffies ?
			                   jiffies_to_msecs(ctx->timeout - _jiffies) * 1000 : 0;
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
		Timer(Server::Entrypoint &ep, unsigned long &jiffies)
		:
			_jiffies(jiffies),
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
			_jiffies = msecs_to_jiffies(_timer_conn.elapsed_ms());
		}

		/**
		 * Get first timer context
		 */
		Context* first() { return _list.first(); }

		unsigned long jiffies() const { return _jiffies; }
};


void Lx::timer_update_jiffies()
{
	timer().update_jiffies();
}


void Lx::run_timer(void *)
{
	Timer &t = timer();

	while (1) {
		Lx::scheduler().current()->block_and_schedule();

		while (Lx::Timer::Context *ctx = t.first()) {
			if (ctx->timeout > t.jiffies())
				break;

			ctx->timer->function(ctx->timer->data);
			t.del(ctx->timer);
		}

		t.schedule_next();
	}
}


Lx::Timer &Lx::_timer_impl(Server::Entrypoint *ep, unsigned long *jiffies_ptr)
{
	static bool initialized = false;

	if (!initialized && !ep) {
		PERR("attempt to use Lx::Timer before its construction");
		class Lx_timer_not_constructed { };
		throw Lx_timer_not_constructed();
	}

	static Lx::Timer inst(*ep, *jiffies_ptr);
	initialized = true;
	return inst;
}


#endif /* _LX_EMUL__IMPL__INTERNAL__TIMER_H_ */
