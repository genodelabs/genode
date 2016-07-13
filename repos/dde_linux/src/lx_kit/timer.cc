/*
 * \brief  Timer
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/server.h>
#include <base/tslab.h>
#include <timer_session/connection.h>

/* Linux kit includes */
#include <lx_kit/internal/list.h>
#include <lx_kit/scheduler.h>

/* Linux emulation environment includes */
#include <lx_emul.h>

#include <lx_kit/timer.h>


namespace Lx_kit { class Timer; }

class Lx_kit::Timer : public Lx::Timer
{
	public:

		/**
		 * Context encapsulates a regular linux timer_list
		 */
		struct Context : public Lx_kit::List<Context>::Element
		{
			enum { INVALID_TIMEOUT = ~0UL };

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

		unsigned long                               &_jiffies;
		::Timer::Connection                          _timer_conn;
		Lx_kit::List<Context>                        _list;
		Lx::Task                                     _timer_task;
		Genode::Signal_rpc_member<Lx_kit::Timer>     _dispatcher;
		Genode::Tslab<Context, 32 * sizeof(Context)> _timer_alloc;
		Lx::jiffies_update_func                      _jiffies_func = nullptr;

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
			_timer_task(Timer::run_timer, reinterpret_cast<void*>(this),
			            "timer", Lx::Task::PRIORITY_2, Lx::scheduler()),
			_dispatcher(ep, *this, &Lx_kit::Timer::_handle),
			_timer_alloc(Genode::env()->heap())
		{
			_timer_conn.sigh(_dispatcher);
		}

		Context* first() { return _list.first(); }

		unsigned long jiffies() const { return _jiffies; }

		static void run_timer(void *p)
		{
			Timer &t = *reinterpret_cast<Timer*>(p);

			while (1) {
				Lx::scheduler().current()->block_and_schedule();

				while (Lx_kit::Timer::Context *ctx = t.first()) {
					if (ctx->timeout > t.jiffies()) {
						break;
					}

					ctx->pending = false;
					ctx->function();

					if (!ctx->pending) {
						t.del(ctx->timer);
					}
				}

				t.schedule_next();
			}
		}

		/*************************
		 ** Lx::Timer interface **
		 *************************/
		void add(void *timer, Type type)
		{
			Context *t = nullptr;

			if (type == HR)
				t = new (&_timer_alloc) Context(static_cast<hrtimer *>(timer));
			else
				t = new (&_timer_alloc) Context(static_cast<timer_list *>(timer));

			_list.append(t);
		}

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

		bool find(void const *timer) const
		{
			for (Context const *c = _list.first(); c; c = c->next())
				if (c->timer == timer)
					return true;

			return false;
		}

		void update_jiffies() {
			_jiffies = _jiffies_func ? _jiffies_func() : msecs_to_jiffies(_timer_conn.elapsed_ms()); }

		void register_jiffies_func(Lx::jiffies_update_func func) {
			_jiffies_func = func; }
};


/******************************
 ** Lx::Timer implementation **
 ******************************/

Lx::Timer &Lx::timer(Server::Entrypoint *ep, unsigned long *jiffies)
{
	static Lx_kit::Timer inst(*ep, *jiffies);
	return inst;
}


void Lx::timer_update_jiffies()
{
	timer().update_jiffies();
}


void Lx::register_jiffies_func(jiffies_update_func func)
{
	dynamic_cast<Lx_kit::Timer &>(timer()).register_jiffies_func(func);
}
