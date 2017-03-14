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

	void handle_slow_timer()
	{
		log("timeout fired");
		done.submit();
	}

	void handle_fast_timer()   { throw Faster_timer_too_slow(); }
	void handle_faster_timer() { set_fast_timers(); }

	void set_fast_timers()
	{
		enum { TIMEOUT_US = 50*1000 };
		fast_timer.trigger_once(TIMEOUT_US);
		faster_timer.trigger_once(TIMEOUT_US/2);
	}

	Lazy_test(Env &env, Signal_context_capability done) : env(env), done(done)
	{
		slow_timer.sigh(slow_handler);
		fast_timer.sigh(fast_handler);
		faster_timer.sigh(faster_handler);

		log("register two-seconds timeout...");
		slow_timer.trigger_once(2*1000*1000);
		set_fast_timers();
	}
};


struct Stress_test
{
	struct Slave
	{
		Signal_handler<Slave> timer_handler;
		Timer::Connection     timer;
		unsigned              us;
		unsigned              count { 0 };

		Slave(Env &env, unsigned ms)
		: timer_handler(env.ep(), *this, &Slave::handle_timer),
		  timer(env), us(ms * 1000) { timer.sigh(timer_handler); }

		virtual ~Slave() { }

		void handle_timer()
		{
			count++;
			timer.trigger_once(us);
		}

		void dump() {
			log("timer (period ", us / 1000, " ms) triggered ", count,
			    " times -> slept ", (us / 1000) * count, " ms"); }

		void start() { timer.trigger_once(us); }
		void stop()  { timer.sigh(Signal_context_capability()); }
	};

	Env                         &env;
	Signal_transmitter           done;
	Heap                         heap    { &env.ram(), &env.rm() };
	Timer::Connection            timer   { env };
	unsigned                     count   { 0 };
	Signal_handler<Stress_test>  handler { env.ep(), *this, &Stress_test::handle };
	Registry<Registered<Slave> > slaves;

	void handle()
	{
		enum { MAX_COUNT = 10 };
		if (count < MAX_COUNT) {
			count++;
			log("wait ", count, "/", (unsigned)MAX_COUNT);
			timer.trigger_once(1000 * 1000);
		} else {
			slaves.for_each([&] (Slave &timer) { timer.stop(); });
			slaves.for_each([&] (Slave &timer) { timer.dump(); });
			done.submit();
		}
	}

	Stress_test(Env &env, Signal_context_capability done) : env(env), done(done)
	{
		timer.sigh(handler);
		for (unsigned ms = 1; ms < 28; ms++) {
			new (heap) Registered<Slave>(slaves, env, ms); }
		slaves.for_each([&] (Slave &slv) { slv.start(); });
		timer.trigger_once(1000 * 1000);
	}

	~Stress_test() {
		slaves.for_each([&] (Registered<Slave> &slv) { destroy(heap, &slv); }); }
};


struct Main
{
	Env                       &env;
	Constructible<Lazy_test>   test_1;
	Signal_handler<Main>       test_1_done { env.ep(), *this, &Main::handle_test_1_done };
	Constructible<Stress_test> test_2;
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
