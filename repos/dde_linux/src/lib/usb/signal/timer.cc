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
#include <timer_session/connection.h>
#include <util/volatile_object.h>

#include <extern_c_begin.h>
#include <lx_emul.h>
#include <extern_c_end.h>

#include "signal.h"
#include "list.h"

unsigned long jiffies;


static void handler(void *timer);

namespace Lx {
	class Timer;
}

static int run_timer(void *);

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
			enum Type { LIST, HR };

			Type               type;
			void              *timer;
			bool               pending { false };
			unsigned long      timeout { INVALID_TIMEOUT }; /* absolute in jiffies */
			bool               programmed { false };

			Context(struct timer_list *timer) : type(LIST), timer(timer) { }
			Context(struct hrtimer    *timer) : type(HR),   timer(timer) { }

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

				case HR:
					{
						hrtimer *t = static_cast<hrtimer *>(timer);
						if (t->function)
							t->function(t);
					}
					break;
				}
			}
		};

	private:

		::Timer::Connection                          _timer_conn;
		Lx::List<Context>                            _list;
		Routine                                     *_timer_task = Routine::add(run_timer, nullptr, "timer");
		Genode::Signal_rpc_member<Lx::Timer>         _dispatcher;
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
			ready = true;
			Routine::schedule_all();
		}

	public:

		/**
		 * Constructor
		 */
		Timer(Server::Entrypoint &ep)
		:
			_dispatcher(ep, *this, &Lx::Timer::_handle),
			_timer_alloc(Genode::env()->heap())
		{
			_timer_conn.sigh(_dispatcher);
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

		Context *find(void const *timer) {
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


Genode::Lazy_volatile_object<Lx::Timer> _lx_timer;


void Timer::init(Server::Entrypoint &ep)
{
	_lx_timer.construct(ep);

	/* initialize value explicitly */
	jiffies = 0UL;
}

void Timer::update_jiffies()
{
	_lx_timer->update_jiffies();
}

static int  run_timer(void *)
{
	while (1) {
		_wait_event(_lx_timer->ready);

		while (Lx::Timer::Context *ctx = _lx_timer->first()) {
			if (ctx->timeout > jiffies)
				break;

			ctx->function();
			_lx_timer->del(ctx->timer);
		}

		_lx_timer->ready = false;
	}

	return 0;
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
}


int timer_pending(const struct timer_list * timer)
{
	bool pending = _lx_timer->pending(timer);
	lx_log(DEBUG_TIMER, "Pending %p %u", timer, pending);
	return pending;
}


int del_timer(struct timer_list *timer)
{
	lx_log(DEBUG_TIMER, "Delete timer %p", timer);
	int rv = _lx_timer->del(timer);
	_lx_timer->schedule_next();

	return rv;
}


/*********************
 ** linux/hrtimer.h **
 *********************/

void hrtimer_init(struct hrtimer *timer, clockid_t clock_id, enum hrtimer_mode mode) { }


int hrtimer_start_range_ns(struct hrtimer *timer, ktime_t tim,
                           unsigned long delta_ns, const enum hrtimer_mode mode)
{
	unsigned long expires = tim.tv64 / (NSEC_PER_MSEC * HZ);

	if (!_lx_timer->find(timer))
		_lx_timer->add(timer);

	lx_log(DEBUG_TIMER, "HR: e: %lu j %lu", jiffies, expires);
	return _lx_timer->schedule(timer, expires);
}


int hrtimer_cancel(struct hrtimer *timer)
{
	int rv = _lx_timer->del(timer);
	_lx_timer->schedule_next();

	return rv;
}
