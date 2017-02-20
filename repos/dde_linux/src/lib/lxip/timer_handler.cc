/*
 * \brief  Signal context for timer events
 * \author Sebastian Sumpf
 * \author Emery Hemingway
 * \author Christian Helmuth
 * \date   2012-05-23
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/log.h>
#include <base/tslab.h>
#include <os/server.h>
#include <timer_session/connection.h>
#include <util/reconstructible.h>

/* Linux kit includes */
#include <lx_kit/internal/list.h>

/* local includes */
#include <lx_emul.h>
#include <lx.h>


/*********************
 ** linux/jiffies.h **
 *********************/

unsigned long jiffies;

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
		struct Context : public Lx_kit::List<Context>::Element
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
		Lx_kit::List<Context>                        _list;
		Genode::Signal_handler<Lx::Timer>            _handler;
		Genode::Tslab<Context, 32 * sizeof(Context)> _timer_alloc;

		void (*_tick)();

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
		void _handle()
		{
			update_jiffies();

			while (Lx::Timer::Context *ctx = _list.first()) {
				if (ctx->timeout > jiffies)
					break;

				ctx->function();
				del(ctx->timer);
			}

			/* tick the higher layer of the component */
			_tick();
		}

	public:

		/**
		 * Constructor
		 */
		Timer(Genode::Env &env, Genode::Entrypoint &ep, Genode::Allocator &alloc,
		      void (*tick)())
		:
			_timer_conn(env),
			_handler(ep, *this, &Lx::Timer::_handle),
			_timer_alloc(&alloc),
			_tick(tick)
		{
			_timer_conn.sigh(_handler);
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
				Genode::error("schedule unknown timer ", timer);
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
};


static Lx::Timer *_timer;


void Lx::timer_init(Genode::Env &env, Genode::Entrypoint &ep,
                    Genode::Allocator &alloc, void (*tick)())
{
	static Lx::Timer inst(env, ep, alloc, tick);
	_timer = &inst;
}


void update_jiffies()
{
	_timer->update_jiffies();
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

	if (!_timer->find(timer))
		_timer->add(timer);

	return _timer->schedule(timer, expires);
}


void setup_timer(struct timer_list *timer,void (*function)(unsigned long),
                 unsigned long data)
{
	timer->function = function;
	timer->data     = data;
}


int timer_pending(const struct timer_list * timer)
{
	bool pending = _timer->pending(timer);
	lx_log(DEBUG_TIMER, "Pending %p %u", timer, pending);
	return pending;
}


int del_timer(struct timer_list *timer)
{
	update_jiffies();
	lx_log(DEBUG_TIMER, "Delete timer %p", timer);
	int rv = _timer->del(timer);
	_timer->schedule_next();

	return rv;
}
