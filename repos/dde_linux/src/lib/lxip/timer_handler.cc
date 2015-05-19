/*
 * \brief  Signal context for timer events
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-05-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/printf.h>
#include <base/tslab.h>
#include <os/server.h>
#include <timer_session/connection.h>
#include <util/volatile_object.h>

#include <lx/extern_c_begin.h>
#include <lx_emul.h>
#include <lx/extern_c_end.h>

#include <env.h>

#include <lx/list.h>


/*********************
 ** linux/jiffies.h **
 *********************/

unsigned long jiffies;

unsigned long msecs_to_jiffies(const unsigned int m) { return m / (1000 / HZ); }

/**
 * Lx::Timer
 */
namespace Lx {
	class Timer;
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
			enum Type { LIST };

			Type               type;
			void              *timer;
			bool               pending { false };
			unsigned long      timeout { INVALID_TIMEOUT }; /* absolute in jiffies */
			bool               programmed { false };

			Context(struct timer_list *timer) : type(LIST), timer(timer) { }

			void expires(unsigned long e)
			{
				if (type == LIST)
					static_cast<timer_list *>(timer)->expires = e;
			}

			void function()
			{
				switch (type) {
				case LIST:
					{
						timer_list *t = static_cast<timer_list *>(timer);
						if (t->function)
							t->function(t->data);
					}
					break;

				}
			}
		};

	private:

		::Timer::Connection                          _timer_conn;
		Lx::List<Context>                            _list;
		Genode::Signal_dispatcher<Lx::Timer>         _dispatcher;
		Genode::Tslab<Context, 32 * sizeof(Context)> _timer_alloc;

	public:

		bool                                          ready  { true };

	private:

		/**
		 * Lookup local timer
		 */
		Context *_find_context(void const *timer)
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
			 * struct timer_list because the checks
			 * it directly.
			 */
			ctx->expires(expires);

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
			update_jiffies();

			while (Lx::Timer::Context *ctx = _list.first()) {
			if (ctx->timeout > jiffies)
					break;

				ctx->function();
				del(ctx->timer);
			}
		}

	public:

		/**
		 * Constructor
		 */
		Timer()
		:
			_dispatcher(*Net::Env::receiver(), *this, &Lx::Timer::_handle),
			_timer_alloc(Genode::env()->heap())
		{
			_timer_conn.sigh(_dispatcher);
			jiffies = 0;
		}

		/**
		 * Add new linux timer
		 */
		template <typename TIMER>
		void add(TIMER *timer)
		{
			Context *t = new (&_timer_alloc) Context(timer);
			_list.append(t);
		}

		/**
		 * Delete linux timer
		 */
		int del(void *timer)
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
		int schedule(void *timer, unsigned long expires)
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
		bool pending(void const *timer)
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

		static Timer &t()
		{
			static Lx::Timer _t;
			return _t;
		}

};


void update_jiffies()
{
	Lx::Timer::t().update_jiffies();
}


/*******************
 ** linux/timer.h **
 *******************/

void init_timer(struct timer_list *timer) { }

void add_timer(struct timer_list *timer)
{
	BUG_ON(timer_pending(timer));
	mod_timer(timer, timer->expires);
}



int mod_timer(struct timer_list *timer, unsigned long expires)
{
	update_jiffies();

	if (!Lx::Timer::t().find(timer))
		Lx::Timer::t().add(timer);

	return Lx::Timer::t().schedule(timer, expires);
}


void setup_timer(struct timer_list *timer,void (*function)(unsigned long),
                 unsigned long data)
{
	timer->function = function;
	timer->data     = data;
}


int timer_pending(const struct timer_list * timer)
{
	bool pending = Lx::Timer::t().pending(timer);
	lx_log(DEBUG_TIMER, "Pending %p %u", timer, pending);
	return pending;
}


int del_timer(struct timer_list *timer)
{
	update_jiffies();
	lx_log(DEBUG_TIMER, "Delete timer %p", timer);
	int rv = Lx::Timer::t().del(timer);
	Lx::Timer::t().schedule_next();

	return rv;
}


