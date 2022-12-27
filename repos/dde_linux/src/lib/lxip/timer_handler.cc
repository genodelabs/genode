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
#include <timer_session/connection.h>
#include <util/reconstructible.h>

/* Linux kit includes */
#include <legacy/lx_kit/internal/list.h>

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

		Genode::Entrypoint                          &_ep;
		::Timer::Connection                         &_timer;

		/* One-shot timeout for timer list */
		::Timer::One_shot_timeout<Lx::Timer>         _timers_one_shot {
			_timer, *this, &Lx::Timer::_handle_timers };

		/* One-shot timeout for 'wait' */
		::Timer::One_shot_timeout<Lx::Timer>         _wait_one_shot {
			_timer, *this, &Lx::Timer::_handle_wait };

		Lx_kit::List<Context>                        _list;
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
		 */
		void _program_first_timer()
		{
			Context *ctx = _list.first();
			if (!ctx)
				return;

			/* calculate relative microseconds for trigger */
			Genode::uint64_t us = ctx->timeout > jiffies ?
			                      (Genode::uint64_t)jiffies_to_msecs(ctx->timeout - jiffies) * 1000 : 0;
			_timers_one_shot.schedule(Genode::Microseconds{us});
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

		inline void _upate_jiffies(Genode::Duration dur)
		{
			auto new_jiffies = usecs_to_jiffies(dur.trunc_to_plain_us().value);
			if (new_jiffies < jiffies)
				jiffies = usecs_to_jiffies(_timer.curr_time().trunc_to_plain_us().value);
			else
				jiffies = new_jiffies;
		}

		/**
		 * Check timers and wake application
		 */
		void _handle_timers(Genode::Duration dur)
		{
			_upate_jiffies(dur);

			while (Lx::Timer::Context *ctx = _list.first()) {
				if (ctx->timeout > jiffies)
					break;

				ctx->pending = false;
				ctx->function();

				if (!ctx->pending)
					del(ctx->timer);
			}

			/* tick the higher layer of the component */
			_tick();
		}

		void _handle_wait(Genode::Duration dur) { _upate_jiffies(dur); }

	public:

		/**
		 * Constructor
		 */
		Timer(Genode::Entrypoint  &ep,
		      ::Timer::Connection &timer,
		      Genode::Allocator   &alloc,
		      void (*tick)())
		:
			_ep(ep),
			_timer(timer),
			_timer_alloc(&alloc),
			_tick(tick)
		{
			update_jiffies();
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

			int rv = ctx->pending ? 1 : 0;

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
			int rv = ctx->pending ? 1 : 0;

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
			/*
			 * Do not use lx_emul usecs_to_jiffies(unsigned int) because
			 * of implicit truncation!
			 */
			jiffies = _timer.curr_time().trunc_to_plain_ms().value / JIFFIES_TICK_MS;
		}

		/**
		 * Get first timer context
		 */
		Context* first() { return _list.first(); }

		void wait(unsigned long timeo = 0)
		{
			/**
			 * XXX
			 * in contrast to wait_uninterruptible(), wait() should be interruptible
			 * although we return immediately once we dispatched any signal, we
			 * need to reflect this via signal_pending()
			 */

			if (timeo > 0)
				_wait_one_shot.schedule(Genode::Microseconds(jiffies_to_usecs(timeo)));

			_ep.wait_and_dispatch_one_io_signal();
			/* update jiffies if we dispatched another signal */
			if (_wait_one_shot.scheduled())
				update_jiffies();
		}

		void wait_uninterruptible(unsigned long timeo)
		{
			if (timeo > 0) {
				_wait_one_shot.schedule(Genode::Microseconds(jiffies_to_usecs(timeo)));
				while (_wait_one_shot.scheduled())
					_ep.wait_and_dispatch_one_io_signal();
			}
		}
};


static Lx::Timer *_timer;


void Lx::timer_init(Genode::Entrypoint  &ep,
                    ::Timer::Connection &timer,
                    Genode::Allocator   &alloc,
                    void (*tick)())
{
	static Lx::Timer inst(ep, timer, alloc, tick);
	_timer = &inst;
}


void update_jiffies() { _timer->update_jiffies(); }

void Lx::timer_update_jiffies() { update_jiffies(); }

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


/*******************
 ** linux/sched.h **
 *******************/

signed long schedule_timeout(signed long timeout)
{
	unsigned long expire = timeout + jiffies;

	/**
	 * XXX
	 * schedule_timeout is called from sock_wait_for_wmem() (UDP) and
	 * sk_stream_wait_memory() (TCP) if sk_wmem_alloc (UDP) resp.
	 * sk_wmem_queued (TCP) reaches are certain threshold
	 * unfortunately, recovery from this state seems to be broken
	 * so that we land here for every skb once we hit the threshold
	 */
	static bool warned = false;
	if (!warned) {
		Genode::warning(__func__, " called (tx throttled?)");
		warned = true;
	}

	long start = jiffies;
	_timer->wait(timeout);
	timeout -= jiffies - start;

	return timeout < 0 ? 0 : timeout;
}

long schedule_timeout_uninterruptible(signed long timeout)
{
	_timer->wait_uninterruptible(timeout);
	return 0;
}

void poll_wait(struct file * filp, wait_queue_head_t * wait_address, poll_table *p)
{
	_timer->wait();
}

bool poll_does_not_wait(const poll_table *p)
{
	return p == nullptr;
}


/******************
 ** linux/time.h **
 ******************/

unsigned long get_seconds(void)
{
	return jiffies / HZ;
}


/*******************
 ** linux/timer.h **
 *******************/

static unsigned long round_jiffies(unsigned long j, bool force_up)
{
	unsigned remainder = j % HZ;

	/*
	 * from timer.c
	 *
	 * If the target jiffie is just after a whole second (which can happen
	 * due to delays of the timer irq, long irq off times etc etc) then
	 * we should round down to the whole second, not up. Use 1/4th second
	 * as cutoff for this rounding as an extreme upper bound for this.
	 * But never round down if @force_up is set.
	 */

	/* per default round down */
	j = j - remainder;

	/* round up if remainder more than 1/4 second (or if we're forced to) */
	if (remainder >= HZ/4 || force_up)
		j += HZ;

	return j;
}

unsigned long round_jiffies(unsigned long j)
{
	return round_jiffies(j, false);
}


unsigned long round_jiffies_up(unsigned long j)
{
	return round_jiffies(j, true);
}


unsigned long round_jiffies_relative(unsigned long j)
{
	return round_jiffies(j + jiffies, false) - jiffies;
}
