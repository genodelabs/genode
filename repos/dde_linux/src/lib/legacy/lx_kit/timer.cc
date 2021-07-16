/*
 * \brief  Timer
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/tslab.h>
#include <timer_session/connection.h>

/* Linux kit includes */
#include <legacy/lx_kit/internal/list.h>
#include <legacy/lx_kit/scheduler.h>

/* Linux emulation environment includes */
#include <lx_emul.h>

#include <legacy/lx_kit/timer.h>


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
						if (t->function) {
							/*
							 * Pass 't->data' instead of 't' for compatibility
							 * with 4.4.3 drivers. When set up with
							 * 'timer_setup()', 't->data' matches 't'.
							 */
							t->function((struct timer_list*)t->data);
						}
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
		::Timer::Connection                          _timer_conn_modern;
		Lx_kit::List<Context>                        _list;
		Lx::Task                                     _timer_task;
		Genode::Signal_handler<Lx_kit::Timer>        _dispatcher;
		Genode::Tslab<Context, 32 * sizeof(Context)> _timer_alloc;

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
		 */
		void _program_first_timer()
		{
			Context *ctx = _list.first();
			if (!ctx)
				return;

			/* calculate relative microseconds for trigger */
			Genode::uint64_t us = ctx->timeout > _jiffies ?
			                      (Genode::uint64_t)jiffies_to_msecs(ctx->timeout - _jiffies) * 1000 : 0;
			_timer_conn.trigger_once(us);
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

			/*
			 * Also write the timeout value to the expires field in
			 * struct timer_list because some code the checks
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
			_timer_task.unblock();

			Lx::scheduler().schedule();
		}

	public:

		/**
		 * Constructor
		 */
		Timer(Genode::Env &env, Genode::Entrypoint &ep,
		      Genode::Allocator &alloc, unsigned long &jiffies)
		:
			_jiffies(jiffies),
			_timer_conn(env),
			_timer_conn_modern(env),
			_timer_task(Timer::run_timer, reinterpret_cast<void*>(this),
			            "timer", Lx::Task::PRIORITY_2, Lx::scheduler()),
			_dispatcher(ep, *this, &Lx_kit::Timer::_handle),
			_timer_alloc(&alloc)
		{
			_timer_conn.sigh(_dispatcher);
			update_jiffies();
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

			int rv = ctx->pending ? 1 : 0;

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
			int rv = ctx->pending ? 1 : 0;

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

		void update_jiffies()
		{
			/*
			 * Do not use lx_emul usecs_to_jiffies(unsigned int) because
			 * of implicit truncation!
			 */
			_jiffies = (Genode::uint64_t)_timer_conn_modern.curr_time().trunc_to_plain_ms().value / JIFFIES_TICK_MS;
		}

		void usleep(Genode::uint64_t us) {
			_timer_conn.usleep(us); }
};


/******************************
 ** Lx::Timer implementation **
 ******************************/

Lx::Timer &Lx::timer(Genode::Env *env, Genode::Entrypoint *ep,
                     Genode::Allocator *md_alloc,
                     unsigned long *jiffies)
{
	static Lx_kit::Timer inst(*env, *ep, *md_alloc, *jiffies);
	return inst;
}


void Lx::timer_update_jiffies()
{
	timer().update_jiffies();
}
