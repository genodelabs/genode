/*
 * \brief  Test for timer service
 * \author Norman Feske
 * \author Martin Stein
 * \date   2009-06-22
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <timer_session/connection.h>
#include <base/registry.h>

using namespace Genode;


struct Lazy_test
{
	struct Faster_timer_too_slow : Exception { };

	Env                      &env;
	Signal_transmitter        done;
	Timer::Connection         slow_timer     { env };
	Signal_handler<Lazy_test> slow_handler   { env.ep(), *this,
	                                           &Lazy_test::handle_slow_timer };
	Timer::Connection         fast_timer     { env };
	Signal_handler<Lazy_test> fast_handler   { env.ep(), *this,
	                                           &Lazy_test::handle_fast_timer };
	Timer::Connection         faster_timer   { env };
	Signal_handler<Lazy_test> faster_handler { env.ep(), *this,
	                                           &Lazy_test::handle_faster_timer };

	enum { RUN_TIME_US = 4*1000*1000, TIMEOUT_US = 200*1000, FACTOR = 8 };
	unsigned fast = 0;
	unsigned faster = 0;

	void handle_slow_timer()
	{
		log("timeout fired - ", fast, "/", faster, "/",
		    RUN_TIME_US / TIMEOUT_US * FACTOR);

		if (fast)
			throw Faster_timer_too_slow();

		done.submit();
	}

	void handle_fast_timer()
	{
		fast ++;
		if (faster <= fast)
			throw Faster_timer_too_slow();
	}

	void handle_faster_timer() { set_fast_timers(); }

	void set_fast_timers()
	{
		fast_timer.trigger_once(TIMEOUT_US);
		faster_timer.trigger_once(TIMEOUT_US/FACTOR);
		faster ++;
	}

	Lazy_test(Env &env, Signal_context_capability done) : env(env), done(done)
	{
		slow_timer.sigh(slow_handler);
		fast_timer.sigh(fast_handler);
		faster_timer.sigh(faster_handler);

		log("register ", RUN_TIME_US/1000/1000, "-seconds timeout...");
		slow_timer.trigger_once(RUN_TIME_US);
		set_fast_timers();
	}
};


struct Stress_test
{
	enum { DURATION_SEC      = 10 };
	enum { MAX_SLV_PERIOD_US = 33000 };

	struct Starvation                    : Exception { };
	struct Violation_of_timer_rate_limit : Exception { };

	struct Slave
	{
		static constexpr int DURATION_US         = DURATION_SEC * 1000 * 1000;
		static constexpr int MIN_TIMER_PERIOD_US = 250;
		static constexpr int MAX_CNT_BASE        = DURATION_US / MIN_TIMER_PERIOD_US;
		static constexpr int MAX_CNT_TOLERANCE   = MAX_CNT_BASE / 9;
		static constexpr int MAX_CNT             = MAX_CNT_BASE + MAX_CNT_TOLERANCE;
		static constexpr int MIN_CNT             = DURATION_US / MAX_SLV_PERIOD_US / 2;

		Signal_handler<Slave> timer_handler;
		Timer::Connection     timer;
		uint64_t              us;
		unsigned              count { 0 };

		Slave(Env &env, uint64_t us)
		: timer_handler(env.ep(), *this, &Slave::handle_timer),
		  timer(env), us(us) { timer.sigh(timer_handler); }

		virtual ~Slave() { }

		void handle_timer()
		{
			count++;
			timer.trigger_once(us);
		}

		void dump(unsigned &starvation, unsigned &rate_violation)
		{
			log("timer (period ", us, " us) triggered ", count,
			    " times (min ", (unsigned)MIN_CNT,
			           " max ", (unsigned)MAX_CNT, ") -> slept ",
			    ((uint64_t)us * count) / 1000, " ms");

			/* detect starvation of timeouts */
			if (count < MIN_CNT) {
				error("triggered less than ", (unsigned)MIN_CNT,
				      " times");
				starvation ++;
			}
			/* detect violation of timer rate limitation */
			if (count > MAX_CNT) {
				error("triggered more than ", (unsigned)MAX_CNT,
				      " times");
				rate_violation ++;
			}
		}

		void start() { timer.trigger_once(us); }
		void stop()  { timer.sigh(Signal_context_capability()); }
	};

	Env                         &env;
	Signal_transmitter           done;
	Heap                         heap    { &env.ram(), &env.rm() };
	Timer::Connection            timer   { env };
	unsigned                     count   { 0 };
	Signal_handler<Stress_test>  handler { env.ep(), *this, &Stress_test::handle };
	Registry<Registered<Slave> > slaves  { };

	void handle()
	{
		if (count < DURATION_SEC) {
			count++;
			log("wait ", count, "/", (uint64_t)DURATION_SEC);
			timer.trigger_once((uint64_t)1000 * 1000);
		} else {
			unsigned starvation = 0;
			unsigned rate_violation = 0;

			slaves.for_each([&] (Slave &timer) { timer.stop(); });
			slaves.for_each([&] (Slave &timer) {
				timer.dump(starvation, rate_violation);
			});

			if (starvation)
				throw Starvation();
			if (rate_violation)
				throw Violation_of_timer_rate_limit();

			done.submit();
		}
	}

	Stress_test(Env &env, Signal_context_capability done) : env(env), done(done)
	{
		timer.sigh(handler);

		for (uint64_t us_1 = 1; us_1 < MAX_SLV_PERIOD_US; us_1 *= 2) {
			new (heap) Registered<Slave>(slaves, env, us_1 - us_1 / 3);
			new (heap) Registered<Slave>(slaves, env, us_1);
		}

		slaves.for_each([&] (Slave &slv) { slv.start(); });
		timer.trigger_once((uint64_t)1000 * 1000);
	}

	~Stress_test() {
		slaves.for_each([&] (Registered<Slave> &slv) { destroy(heap, &slv); }); }
};


struct Main
{
	Env                       &env;
	Constructible<Lazy_test>   test_1      { };
	Signal_handler<Main>       test_1_done { env.ep(), *this, &Main::handle_test_1_done };
	Constructible<Stress_test> test_2      { };
	Signal_handler<Main>       test_2_done { env.ep(), *this, &Main::handle_test_2_done };

	void handle_test_1_done()
	{
		test_1.destruct();
		test_2.construct(env, test_2_done);
	}

	void handle_test_2_done()
	{
		log("--- timer test finished ---");
		env.parent().exit(0);
	}

	Main(Env &env) : env(env)
	{
		log("--- timer test ---");
		test_1.construct(env, test_1_done);
	}
};


void Component::construct(Env &env) { static Main main(env); }
