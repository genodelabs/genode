/*
 * \brief  Test for timeout library
 * \author Martin Stein
 * \date   2016-11-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_ram_dataspace.h>
#include <timer_session/connection.h>
#include <util/fifo.h>
#include <util/misc_math.h>

using namespace Genode;


struct Test
{
	Env               &env;
	unsigned          &error_cnt;
	Signal_transmitter done;
	unsigned           id;
	Timer::Connection  timer_connection { env };
	Timer::Connection  timer            { env };

	Test(Env                       &env,
	     unsigned                  &error_cnt,
	     Signal_context_capability  done,
	     unsigned                   id,
	     char const                *brief)
	:
		env(env), error_cnt(error_cnt), done(done), id(id)
	{
		/*
		 * FIXME Activate interpolation early to give it some time to
		 *       calibrate. Otherwise, we may get non-representative
		 *       results in at least the fast-polling test, which starts
		 *       directly with the heaviest load. This is only necessary
		 *       because Timer::Connection by now must be backwards compatible
		 *       and therefore starts interpolation only on demand.
		 */
		timer.curr_time();

		log("\nTEST ", id, ": ", brief, "\n");
	}

	float percentage(unsigned long value, unsigned long base)
	{
		/*
		 * FIXME When base = 0 and value != 0, we normally want to return
		 *       FLT_MAX but this causes a compile error. Thus, we use a
		 *       pretty high value instead.
		 */
		return base ? ((float)value / base * 100) : value ? 1000000 : 0;
	}

	~Test() { log("\nTEST ", id, " finished\n"); }
};


struct Mixed_timeouts : Test
{
	static constexpr char const *brief = "schedule multiple timeouts simultaneously";

	enum { NR_OF_EVENTS   = 20 };
	enum { NR_OF_TIMEOUTS = 4 };
	enum { MAX_ERROR_PC   = 10 };

	struct Timeout
	{
		char         const *const name;
		Microseconds const        us;
	};

	struct Event
	{
		Timeout  const *const timeout;
		Duration const        time;
	};

	Timeout const timeouts[NR_OF_TIMEOUTS] {
		{ "Periodic  700 ms", Microseconds( 700000) },
		{ "Periodic 1000 ms", Microseconds(1000000) },
		{ "One-shot 3250 ms", Microseconds(3250000) },
		{ "One-shot 5200 ms", Microseconds(5200000) }
	};

	/*
	 * We want to check only timeouts that have a distance of at least
	 * 200ms to each other timeout. Thus, the items in this array that
	 * have an empty name are treated as wildcards and match any timeout.
	 */
	Event const events[NR_OF_EVENTS] {
		{ nullptr,      Duration(Milliseconds(0))    },
		{ nullptr,      Duration(Milliseconds(0))    },
		{ &timeouts[0], Duration(Milliseconds(700))  },
		{ &timeouts[1], Duration(Milliseconds(1000)) },
		{ &timeouts[0], Duration(Milliseconds(1400)) },
		{ nullptr,      Duration(Milliseconds(2000)) },
		{ nullptr,      Duration(Milliseconds(2100)) },
		{ &timeouts[0], Duration(Milliseconds(2800)) },
		{ &timeouts[1], Duration(Milliseconds(3000)) },
		{ &timeouts[2], Duration(Milliseconds(3250)) },
		{ &timeouts[0], Duration(Milliseconds(3500)) },
		{ &timeouts[1], Duration(Milliseconds(4000)) },
		{ &timeouts[0], Duration(Milliseconds(4200)) },
		{ nullptr,      Duration(Milliseconds(4900)) },
		{ nullptr,      Duration(Milliseconds(5000)) },
		{ &timeouts[3], Duration(Milliseconds(5200)) },
		{ &timeouts[0], Duration(Milliseconds(5600)) },
		{ &timeouts[1], Duration(Milliseconds(6000)) },
		{ &timeouts[0], Duration(Milliseconds(6300)) },
		{ &timeouts[2], Duration(Milliseconds(6500)) }
	};

	Duration init_time { Microseconds(0) };
	unsigned event_id  { 0 };

	Timer::Periodic_timeout<Mixed_timeouts> pt1 { timer, *this, &Mixed_timeouts::handle_pt1, timeouts[0].us };
	Timer::Periodic_timeout<Mixed_timeouts> pt2 { timer, *this, &Mixed_timeouts::handle_pt2, timeouts[1].us };
	Timer::One_shot_timeout<Mixed_timeouts> ot1 { timer, *this, &Mixed_timeouts::handle_ot1 };
	Timer::One_shot_timeout<Mixed_timeouts> ot2 { timer, *this, &Mixed_timeouts::handle_ot2 };

	void handle_pt1(Duration time) { handle(time, timeouts[0]); }
	void handle_pt2(Duration time) { handle(time, timeouts[1]); }
	void handle_ot1(Duration time) { handle(time, timeouts[2]); ot1.schedule(timeouts[2].us); }
	void handle_ot2(Duration time) { handle(time, timeouts[3]); }

	void handle(Duration time, Timeout const &timeout)
	{
		if (event_id == NR_OF_EVENTS) {
			return; }

		if (!event_id) {
			init_time = time; }

		Event const &event = events[event_id++];
		unsigned long time_us = time.trunc_to_plain_us().value -
		                        init_time.trunc_to_plain_us().value;

		unsigned long event_time_us = event.time.trunc_to_plain_us().value;
		unsigned long error_us      = max(time_us, event_time_us) -
		                              min(time_us, event_time_us);

		float const error_pc = percentage(error_us, timeout.us.value);

		log(time_us / 1000UL, " ms: ", timeout.name, " timeout triggered,"
		    " error ", error_us, " us (", error_pc, " %)");

		if (error_pc > MAX_ERROR_PC) {

			error("absolute timeout error greater than ", (unsigned)MAX_ERROR_PC, " %");
			error_cnt++;
		}
		if (event.timeout && event.timeout != &timeout) {

			error("expected timeout ", timeout.name);
			error_cnt++;
		}
		if (event_id == NR_OF_EVENTS) {
			done.submit(); }
	}

	Mixed_timeouts(Env                       &env,
	               unsigned                  &error_cnt,
	               Signal_context_capability  done,
	               unsigned                   id)
	:
		Test(env, error_cnt, done, id, brief)
	{
		ot1.schedule(timeouts[2].us);
		ot2.schedule(timeouts[3].us);
	}
};


struct Fast_polling : Test
{
	static constexpr char const *brief = "poll time pretty fast";

	enum { NR_OF_ROUNDS          = 4 };
	enum { MIN_ROUND_DURATION_MS = 2000 };
	enum { MAX_NR_OF_POLLS       = 10000000 };
	enum { MIN_NR_OF_POLLS       = 1000 };
	enum { STACK_SIZE            = 4 * 1024 * sizeof(addr_t) };
	enum { MIN_TIME_COMPARISONS  = 100 };
	enum { MAX_TIME_ERR_US       = 10000 };
	enum { MAX_AVG_TIME_ERR_US   = 1000 };
	enum { MAX_DELAY_ERR_US      = 2000 };
	enum { MAX_AVG_DELAY_ERR_US  = 20 };
	enum { MAX_POLL_LATENCY_US   = 1000 };
	enum { BUF_SIZE              = MAX_NR_OF_POLLS * sizeof(unsigned long) };

	Entrypoint                   main_ep;
	Signal_handler<Fast_polling> main_handler;

	Attached_ram_dataspace local_us_buf_1 { env.ram(), env.rm(), BUF_SIZE };
	Attached_ram_dataspace local_us_buf_2 { env.ram(), env.rm(), BUF_SIZE };
	Attached_ram_dataspace remote_ms_buf  { env.ram(), env.rm(), BUF_SIZE };
	unsigned long volatile *local_us_1    { local_us_buf_1.local_addr<unsigned long>() };
	unsigned long volatile *local_us_2    { local_us_buf_2.local_addr<unsigned long>() };
	unsigned long volatile *remote_ms     { remote_ms_buf.local_addr<unsigned long>() };

	unsigned const delay_loops_per_poll[NR_OF_ROUNDS] { 1,
	                                                    1000,
	                                                    10000,
	                                                    100000 };

	/*
	 * Accumulates great amounts of integer values to one average value
	 *
	 * Aims for best possible precision with a fixed amount of integer buffers
	 */
	struct Average_accumulator
	{
		private:

			unsigned long _avg     { 0 };
			unsigned long _avg_cnt { 0 };
			unsigned long _acc     { 0 };
			unsigned long _acc_cnt { 0 };

		public:

			void flush()
			{
				unsigned long acc_avg = _acc / _acc_cnt;
				if (!_avg_cnt) { _avg = _avg + acc_avg; }
				else {
					float acc_fac = (float)_acc_cnt / _avg_cnt;
					_avg = (_avg + ((float)acc_fac * acc_avg)) / (1 + acc_fac);
				}
				_avg_cnt += _acc_cnt;
				_acc      = 0;
				_acc_cnt  = 0;
			}

			void add(unsigned long add)
			{
				if (add > (~0UL - _acc)) {
					flush(); }
				_acc += add;
				_acc_cnt++;
			}

			unsigned long avg()
			{
				if (_acc_cnt) {
					flush(); }
				return _avg;
			}

			unsigned long avg_cnt()
			{
				if (_acc_cnt) {
					flush(); }
				return _avg_cnt;
			}
	};

	unsigned long delay_us(unsigned poll)
	{
		return local_us_1[poll - 1] > local_us_1[poll] ?
		       local_us_1[poll - 1] - local_us_1[poll] :
		       local_us_1[poll]     - local_us_1[poll - 1];
	}

	unsigned long estimate_delay_loops_per_ms()
	{
		log("estimate CPU speed ...");
		for (unsigned long max_cnt = 1000UL * 1000UL; ; max_cnt *= 2) {

			/* measure consumed time of a limited busy loop */
			unsigned long volatile start_ms = timer_connection.elapsed_ms();
			for (unsigned long volatile cnt = 0; cnt < max_cnt; cnt++) { }
			unsigned long volatile end_ms = timer_connection.elapsed_ms();

			/*
			 * We only return the result if the loop was time intensive enough
			 * and therefore representative. Otherwise we raise the loop-
			 * counter limit and do a new estimation.
			 */
			unsigned long diff_ms = end_ms - start_ms;
			if (diff_ms > 1000UL) {
				return max_cnt / diff_ms; }
		}
	}

	void main()
	{
		/*
		 * Estimate CPU speed
		 *
		 * The test delays must be done through busy spinning. If we would
		 * use a timer session instead, we could not produce delays of only a
		 * few microseconds. Thus, to get similar delays on each platform we
		 * have to do this estimation.
		 */
		unsigned long volatile delay_loops_per_remote_poll =
			estimate_delay_loops_per_ms() / 100;

		/* do several rounds of the test with different parameters each */
		for (unsigned round = 0; round < NR_OF_ROUNDS; round++) {

			/* print round parameters */
			log("");
			log("--- Round ",    round + 1,
			    ": polling delay ", delay_loops_per_poll[round], " loop(s) ---");
			log("");

			unsigned long volatile delay_loops = 0;

			unsigned long nr_of_polls           = MAX_NR_OF_POLLS;
			unsigned long delay_loops_per_poll_ = delay_loops_per_poll[round];
			unsigned long end_remote_ms         = timer_connection.elapsed_ms() +
			                                      MIN_ROUND_DURATION_MS;

			/* limit polling to our buffer capacity */
			for (unsigned poll = 0; poll < nr_of_polls; poll++) {

				/* create delay between two polls */
				for (unsigned long volatile i = 0; i < delay_loops_per_poll_; i++) { }

				/* count delay loops to limit frequency of remote time reading */
				delay_loops += delay_loops_per_poll_;

				/*
				 * We buffer the results in local variables first so the RAM
				 * access wont raise the delay between the reading of the
				 * different time values.
				 */
				unsigned long volatile local_us_1_;
				unsigned long volatile local_us_2_;
				unsigned long volatile remote_ms_;

				/* read local time before the remote time reading */
				local_us_1_ = timer.curr_time().trunc_to_plain_us().value;

				/*
				 * Limit frequency of remote-time reading
				 *
				 * If we would stress the timer connection to much, the
				 * back-end functionality of the timeout framework would
				 * remarkably slow down which causes a phase of adaption with
				 * bigger errors. But the goal of the framework is to spare
				 * calls to timer connections anyway. So, its fine to limit
				 * the polling frequency here.
				 */
				if (delay_loops > delay_loops_per_remote_poll) {

					/* read remote time and second local time */
					remote_ms_  = timer_connection.elapsed_ms();
					local_us_2_ = timer.curr_time().trunc_to_plain_us().value;

					/* reset delay counter for remote-time reading */
					delay_loops = 0;

				} else {

					/* mark remote-time and second local-time value invalid */
					remote_ms_  = 0;
					local_us_2_ = 0;
				}
				/* store results to the buffers */
				remote_ms[poll]  = remote_ms_;
				local_us_1[poll] = local_us_1_;
				local_us_2[poll] = local_us_2_;

				/* if the minimum round duration is reached, end polling */
				if (remote_ms_ > end_remote_ms) {
					nr_of_polls = poll + 1;
					break;
				}
			}

			/*
			 * Mark results with a bad latency dismissed
			 *
			 * It might be, that we got scheduled away between reading out
			 * local and remote time. This would cause the test result to
			 * be much worse than the actual precision of the timeout
			 * framework. Thus, we ignore such results.
			 */
			unsigned nr_of_good_polls = 0;
			unsigned nr_of_bad_polls = 0;
			for (unsigned poll = 0; poll < nr_of_polls; poll++) {

				if (remote_ms[poll] &&
				    local_us_2[poll] - local_us_1[poll] > MAX_POLL_LATENCY_US)
				{
					local_us_1[poll] = 0;
					nr_of_bad_polls++;

				} else {

					nr_of_good_polls++; }
			}

			/*
			 * Calculate the average delay between consecutive polls
			 * (using the local time).
			 */
			Average_accumulator avg_delay_us;
			for (unsigned poll = 1; poll < nr_of_polls; poll++) {

				/* skip if this result was dismissed */
				if (!local_us_1[poll]) {
					poll++;
					continue;
				}
				/* check if local time is monotone */
				if (local_us_1[poll - 1] > local_us_1[poll]) {

					error("time is not monotone at poll #", poll);
					error_cnt++;
				}
				/* check out delay between this poll and the last one */
				avg_delay_us.add(delay_us(poll));
			}

			/*
			 * Calculate the average and maximum error of the local time
			 * compared to the remote time as well as the duration of the
			 * whole test round.
			 */
			Average_accumulator avg_time_err_us;
			unsigned long       max_time_err_us = 0UL;

			for (unsigned poll = 0; poll < nr_of_polls; poll++) {

				/* skip if this result was dismissed */
				if (!local_us_1[poll]) {
					continue; }

				/* skip if this poll contains no remote time */
				if (!remote_ms[poll]) {
					continue; }

				/* scale-up remote time to microseconds and calculate error */
				if (remote_ms[poll] > ~0UL / 1000UL) {
					error("can not translate remote time to microseconds");
					error_cnt++;
				}
				unsigned long const remote_us   = remote_ms[poll] * 1000UL;
				unsigned long const time_err_us = remote_us > local_us_1[poll] ?
				                                  remote_us - local_us_1[poll] :
				                                  local_us_1[poll] - remote_us;

				/* update max time error */
				if (time_err_us > max_time_err_us) {
					max_time_err_us = time_err_us; }

				/* update average time error */
				avg_time_err_us.add(time_err_us);
			}
			Average_accumulator avg_delay_err_us;
			unsigned long avg_delay_us_ = avg_delay_us.avg();

			/*
			 * Calculate the average error of the delays compared to the
			 * average delay (in microseconds and percent of the average
			 * delay).
			 */
			unsigned long max_delay_err_us = 0;
			for (unsigned poll = 1; poll < nr_of_polls; poll++) {

				/* skip if this result was dismissed */
				if (!local_us_1[poll]) {
					poll++;
					continue;
				}

				unsigned long delay_us_    = delay_us(poll);
				unsigned long delay_err_us = delay_us_ > avg_delay_us_ ?
				                             delay_us_ - avg_delay_us_ :
				                             avg_delay_us_ - delay_us_;

				if (delay_err_us > max_delay_err_us) {
					max_delay_err_us = delay_err_us; }

				avg_delay_err_us.add(delay_err_us);
			}

			unsigned long const max_avg_delay_err_us = (unsigned long)MAX_AVG_DELAY_ERR_US +
			                                           avg_delay_us_ / 20;

			bool const error_nr_of_good_polls = (nr_of_good_polls          < MIN_NR_OF_POLLS);
			bool const error_nr_of_time_cmprs = (avg_time_err_us.avg_cnt() < MIN_TIME_COMPARISONS);
			bool const error_avg_time_err     = (avg_time_err_us.avg()     > MAX_AVG_TIME_ERR_US);
			bool const error_max_time_err     = (max_time_err_us           > MAX_TIME_ERR_US);
			bool const error_avg_delay_err    = (avg_delay_err_us.avg()    > max_avg_delay_err_us);

			error_cnt += error_nr_of_good_polls;
			error_cnt += error_nr_of_time_cmprs;
			error_cnt += error_avg_time_err;
			error_cnt += error_max_time_err;
			error_cnt += error_avg_delay_err;

			log(error_nr_of_good_polls ? "\033[31mbad:  " : "good: ", "nr of good polls       ", nr_of_good_polls,            " (min ", (unsigned)MIN_NR_OF_POLLS,              ")\033[0m");
			log(                                            "      ", "nr of bad polls        ", nr_of_bad_polls                                                                          );
			log(error_nr_of_time_cmprs ? "\033[31mbad:  " : "good: ", "nr of time comparisons ", avg_time_err_us.avg_cnt(),   " (min ", (unsigned)MIN_TIME_COMPARISONS,         ")\033[0m");
			log(error_avg_time_err     ? "\033[31mbad:  " : "good: ", "average time error     ", avg_time_err_us.avg(),    " us (max ", (unsigned long)MAX_AVG_TIME_ERR_US,  " us)\033[0m");
			log(error_max_time_err     ? "\033[31mbad:  " : "good: ", "maximum time error     ", max_time_err_us,          " us (max ", (unsigned long)MAX_TIME_ERR_US,      " us)\033[0m");
			log(                                            "      ", "average delay          ", avg_delay_us.avg(),       " us"                                                          );
			log(error_avg_delay_err    ? "\033[31mbad:  " : "good: ", "average delay error    ", avg_delay_err_us.avg(),   " us (max ", max_avg_delay_err_us,                " us)\033[0m");
			log(                                            "      ", "maximum delay error    ", max_delay_err_us,         " us"                                                          );

		}
		done.submit();
	}

	Fast_polling(Env                       &env,
	             unsigned                  &error_cnt,
	             Signal_context_capability  done,
	             unsigned                   id)
	:
		Test(env, error_cnt, done, id, brief),
		main_ep(env, STACK_SIZE, "fast_polling_ep"),
		main_handler(main_ep, *this, &Fast_polling::main)
	{
		Signal_transmitter(main_handler).submit();
	}
};


struct Main
{
	Env                           &env;
	unsigned                       error_cnt   { 0 };
	Constructible<Fast_polling>    test_1;
	Constructible<Mixed_timeouts>  test_2;
	Signal_handler<Main>           test_1_done { env.ep(), *this, &Main::handle_test_1_done };
	Signal_handler<Main>           test_2_done { env.ep(), *this, &Main::handle_test_2_done };

	Main(Env &env) : env(env)
	{
		test_1.construct(env, error_cnt, test_1_done, 1);
	}

	void handle_test_1_done()
	{
		test_1.destruct();
		test_2.construct(env, error_cnt, test_2_done, 2);
	}

	void handle_test_2_done()
	{
		test_2.destruct();
		if (error_cnt) {
			error("test failed because of ", error_cnt, " error(s)");
			env.parent().exit(-1);
		} else {
			env.parent().exit(0);
		}
	}
};


void Component::construct(Env &env) { static Main main(env); }
