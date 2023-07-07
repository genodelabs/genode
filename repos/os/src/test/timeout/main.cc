/*
 * \brief  Test for the timeout library
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
#include <base/attached_rom_dataspace.h>

using namespace Genode;


/**
 * FIXME
 * This function and its use are a quickfix that avoid refactorization of the
 * timeout framework respectively the timeout test for now. It should be
 * replaced in the near future by a solution that is part of the base lib and
 * thus can be implemented the clean way in platform specific files.
 */
static bool precise_time(Xml_node config)
{
	String<32> attr = config.attribute_value("precise_time", String<32>("false"));
	if (attr == "true") { return true; }
	if (attr == "false") { return false; }
	if (attr == "dynamic") {
#ifdef __x86_64__
		unsigned long cpuid = 0x80000007, edx = 0;
		asm volatile ("cpuid" : "+a" (cpuid), "=d" (edx) : : "rbx", "rcx");
		return edx & 0x100;
#elif __i386__
		unsigned long cpuid = 0x80000007, edx = 0;
		asm volatile ("push %%ebx  \n"
		              "cpuid       \n"
		              "pop  %%ebx" : "+a" (cpuid), "=d" (edx) : : "ecx");
		return edx & 0x100;
#endif
	}
	return false;
}


#pragma GCC push_options
#pragma GCC optimize("O0")
void delay_loop(unsigned long num_iterations)
{
	for (unsigned long idx = 0; idx < num_iterations; idx++) { }
}
#pragma GCC pop_options


struct Test
{
	Env                    &env;
	unsigned               &error_cnt;
	Signal_transmitter      done;
	unsigned                id;
	Attached_rom_dataspace  config { env, "config" };

	Timer::Connection       timer  { env };

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


struct Lock_test : Test
{
	static constexpr char const *brief = "Test locks in handlers";

	bool                                stop     { false };
	Microseconds                        us       { 1000 };
	Mutex                               mutex    { };
	Timer::One_shot_timeout<Lock_test>  ot1      { timer, *this, &Lock_test::handle_ot1 };
	Timer::One_shot_timeout<Lock_test>  ot2      { timer, *this, &Lock_test::handle_ot2 };
	Timer::One_shot_timeout<Lock_test>  ot3      { timer, *this, &Lock_test::handle_ot3 };

	Lock_test(Env                       &env,
	          unsigned                  &error_cnt,
	          Signal_context_capability  done,
	          unsigned                   id)
	:
		Test(env, error_cnt, done, id, brief)
	{
		ot1.schedule(us);
		ot2.schedule(us);
		ot3.schedule(us);
	}

	void handle(Timer::One_shot_timeout<Lock_test> &ot)
	{
		if (stop)
			return;

		if (!us.value) {
			log("good");
			done.submit();
			stop = true;
			return;
		}
		Mutex::Guard mutex_guard(mutex);
		us.value--;
		ot.schedule(us);
	}

	void handle_ot1(Duration) { handle(ot1); }
	void handle_ot2(Duration) { handle(ot2); }
	void handle_ot3(Duration) { handle(ot3); }
};


struct Duration_test : Test
{
	static constexpr char const *brief = "Test operations on durations";

	Duration_test(Env                       &env,
	              unsigned                  &error_cnt,
	              Signal_context_capability  done,
	              unsigned                   id)
	:
		Test(env, error_cnt, done, id, brief)
	{
		log("tests with common duration values");
		enum : uint64_t { US_PER_HOUR = (uint64_t)1000 * 1000 * 60 * 60 };
		enum : uint64_t { US_PER_MS   = (uint64_t)1000 };

		/* create durations for corner cases */
		Duration min   (Microseconds(0));
		Duration hour  (Microseconds((uint64_t)US_PER_HOUR));
		Duration max   (Microseconds(~(uint64_t)0));
		Duration max_ms(Microseconds(~(uint64_t)0 - US_PER_MS + 1));
		{
			/* create durations near the corner cases */
			Duration min_plus_1  (Microseconds(1));
			Duration hour_minus_1(Microseconds((uint64_t)US_PER_HOUR - 1));
			Duration hour_plus_1 (Microseconds((uint64_t)US_PER_HOUR + 1));
			Duration max_minus_1 (Microseconds(~(uint64_t)0 - 1));

			/* all must be greater than the minimum */
			if (min_plus_1  .less_than(min)) { error(__func__, ":", __LINE__); error_cnt++; }
			if (hour_minus_1.less_than(min)) { error(__func__, ":", __LINE__); error_cnt++; }
			if (hour        .less_than(min)) { error(__func__, ":", __LINE__); error_cnt++; }
			if (hour_plus_1 .less_than(min)) { error(__func__, ":", __LINE__); error_cnt++; }
			if (max         .less_than(min)) { error(__func__, ":", __LINE__); error_cnt++; }
			if (max_minus_1 .less_than(min)) { error(__func__, ":", __LINE__); error_cnt++; }

			/* all must be less than the maximum */
			if (max.less_than(min         )) { error(__func__, ":", __LINE__); error_cnt++; }
			if (max.less_than(min_plus_1  )) { error(__func__, ":", __LINE__); error_cnt++; }
			if (max.less_than(hour_minus_1)) { error(__func__, ":", __LINE__); error_cnt++; }
			if (max.less_than(hour        )) { error(__func__, ":", __LINE__); error_cnt++; }
			if (max.less_than(hour_plus_1 )) { error(__func__, ":", __LINE__); error_cnt++; }
			if (max.less_than(max_minus_1 )) { error(__func__, ":", __LINE__); error_cnt++; }

			/* consistency around one hour */
			if (hour       .less_than(hour_minus_1)) { error(__func__, ":", __LINE__); error_cnt++; }
			if (hour_plus_1.less_than(hour        )) { error(__func__, ":", __LINE__); error_cnt++; }
		}
		/* consistency when we double the values */
		Duration two_hours  = hour;
		Duration two_max    = max;
		Duration two_max_ms = max_ms;
		two_hours.add(Microseconds((uint64_t)US_PER_HOUR));
		try {
			two_max.add(Microseconds(1));
			error(__func__, ":", __LINE__); error_cnt++;
		} catch (Duration::Overflow) { }
		try {
			two_max.add(Milliseconds(1));
			error(__func__, ":", __LINE__); error_cnt++;
		} catch (Duration::Overflow) { }
		try {
			two_max_ms.add(Milliseconds(1));
			error(__func__, ":", __LINE__); error_cnt++;
		} catch (Duration::Overflow) { }

		if (two_hours.less_than(hour)) {
			error(__func__, ":", __LINE__); error_cnt++; }

		if (two_max.trunc_to_plain_us().value != max.trunc_to_plain_us().value) {
			error(__func__, ":", __LINE__); error_cnt++; }

		if (two_max_ms.trunc_to_plain_us().value != max_ms.trunc_to_plain_us().value) {
			error(__func__, ":", __LINE__); error_cnt++; }

		/* create durations near corner cases by increasing after construction */
		Duration hour_minus_1(Microseconds((uint64_t)US_PER_HOUR - 2));
		Duration hour_plus_1 (Microseconds((uint64_t)US_PER_HOUR));
		Duration max_minus_1 (Microseconds(~(uint64_t)0 - 2));
		hour_minus_1.add(Microseconds(1));
		hour_plus_1 .add(Microseconds(1));
		max_minus_1 .add(Microseconds(1));

		/* consistency around corner cases */
		if (hour       .less_than(hour_minus_1)) { error(__func__, ":", __LINE__); error_cnt++; }
		if (hour_plus_1.less_than(hour        )) { error(__func__, ":", __LINE__); error_cnt++; }
		if (max        .less_than(max_minus_1 )) { error(__func__, ":", __LINE__); error_cnt++; }

		Test::done.submit();
	}
};


struct Mixed_timeouts : Test
{
	static constexpr char const *brief = "schedule multiple timeouts simultaneously";

	enum { NR_OF_EVENTS   = 21 };
	enum { NR_OF_TIMEOUTS = 6 };

	struct Timeout
	{
		char         const *const name;
		Microseconds const        us;
	};

	struct Timeout_event
	{
		Timeout  const *const timeout;
		Duration const        time;
	};

	/*
	 * Which timeouts we do install and with which configuration
	 *
	 * We mix in timeouts with the maximum duration to see if they trigger any
	 * corner-case bugs. These timeouts are expected to be so big that they
	 * do not trigger during the lifetime of the test.
	 */
	Timeout const timeouts[NR_OF_TIMEOUTS] {
		/* 0 */ { "Periodic  700 ms", Microseconds(      700000) },
		/* 1 */ { "Periodic 1000 ms", Microseconds(     1000000) },
		/* 2 */ { "Periodic  max ms", Microseconds(~(uint64_t)0) },
		/* 3 */ { "One-shot 3250 ms", Microseconds(     3250000) },
		/* 4 */ { "One-shot 5200 ms", Microseconds(     5200000) },
		/* 5 */ { "One-shot  max ms", Microseconds(~(uint64_t)0) },
	};

	/*
	 * Our expectations which timeout should trigger at which point in time
	 *
	 * We want to check only timeouts that have a distance of at least
	 * 200ms to each other timeout. Thus, the items in this array that
	 * have an empty name are treated as wildcards and match any timeout.
	 */
	Timeout_event const events[NR_OF_EVENTS] {
		/*  0 */ { nullptr,      Duration(Milliseconds(   0)) },
		/*  1 */ { nullptr,      Duration(Milliseconds(   0)) },
		/*  2 */ { nullptr,      Duration(Milliseconds(   0)) },
		/*  3 */ { &timeouts[0], Duration(Milliseconds( 700)) },
		/*  4 */ { &timeouts[1], Duration(Milliseconds(1000)) },
		/*  5 */ { &timeouts[0], Duration(Milliseconds(1400)) },
		/*  6 */ { nullptr,      Duration(Milliseconds(2000)) },
		/*  7 */ { nullptr,      Duration(Milliseconds(2100)) },
		/*  8 */ { &timeouts[0], Duration(Milliseconds(2800)) },
		/*  9 */ { &timeouts[1], Duration(Milliseconds(3000)) },
		/* 10 */ { &timeouts[3], Duration(Milliseconds(3250)) },
		/* 11 */ { &timeouts[0], Duration(Milliseconds(3500)) },
		/* 12 */ { &timeouts[1], Duration(Milliseconds(4000)) },
		/* 13 */ { &timeouts[0], Duration(Milliseconds(4200)) },
		/* 14 */ { nullptr,      Duration(Milliseconds(4900)) },
		/* 15 */ { nullptr,      Duration(Milliseconds(5000)) },
		/* 16 */ { &timeouts[4], Duration(Milliseconds(5200)) },
		/* 17 */ { &timeouts[0], Duration(Milliseconds(5600)) },
		/* 18 */ { &timeouts[1], Duration(Milliseconds(6000)) },
		/* 19 */ { &timeouts[0], Duration(Milliseconds(6300)) },
		/* 20 */ { &timeouts[3], Duration(Milliseconds(6500)) }
	};

	struct {
		uint64_t        event_time_us { 0 };
		uint64_t        time_us       { 0 };
		Timeout const * timeout       { nullptr };
	} results [NR_OF_EVENTS];

	Duration init_time    { Microseconds(0) };
	unsigned event_id     { 0 };
	uint64_t max_error_us { config.xml().attribute_value("precise_timeouts", true) ?
	                        (uint64_t)50000 : (uint64_t)200000 };

	Timer::Periodic_timeout<Mixed_timeouts> pt1 { timer, *this, &Mixed_timeouts::handle_pt1, timeouts[0].us };
	Timer::Periodic_timeout<Mixed_timeouts> pt2 { timer, *this, &Mixed_timeouts::handle_pt2, timeouts[1].us };
	Timer::Periodic_timeout<Mixed_timeouts> pt3 { timer, *this, &Mixed_timeouts::handle_pt3, timeouts[2].us };
	Timer::One_shot_timeout<Mixed_timeouts> ot1 { timer, *this, &Mixed_timeouts::handle_ot1 };
	Timer::One_shot_timeout<Mixed_timeouts> ot2 { timer, *this, &Mixed_timeouts::handle_ot2 };
	Timer::One_shot_timeout<Mixed_timeouts> ot3 { timer, *this, &Mixed_timeouts::handle_ot3 };

	void handle_pt1(Duration time) { handle(time, timeouts[0]); }
	void handle_pt2(Duration time) { handle(time, timeouts[1]); }
	void handle_pt3(Duration time) { handle(time, timeouts[2]); }
	void handle_ot1(Duration time) { handle(time, timeouts[3]); ot1.schedule(timeouts[3].us); }
	void handle_ot2(Duration time) { handle(time, timeouts[4]); }
	void handle_ot3(Duration time) { handle(time, timeouts[5]); }

	void handle(Duration time, Timeout const &timeout)
	{
		/* stop if we have received the expected number of events */
		if (event_id == NR_OF_EVENTS) {
			return; }

		/* remember the time of the first event as offset for the others */
		if (!event_id) {
			init_time = time; }

		Timeout_event const &event      = events[event_id];
		results[event_id].event_time_us = event.time.trunc_to_plain_us().value;
		results[event_id].time_us       = time.trunc_to_plain_us().value -
		                                  init_time.trunc_to_plain_us().value;
		results[event_id].timeout       = &timeout;

		if (event.timeout && event.timeout != &timeout) {
			error("expected timeout ", event.timeout->name);
			error_cnt++;
		}

		event_id++;

		if (event_id != NR_OF_EVENTS) {
			return; }

		for (unsigned i = 0; i < NR_OF_EVENTS; i++) {
			uint64_t const event_time_us = results[i].event_time_us;
			uint64_t const time_us       = results[i].time_us;
			uint64_t const error_us      = max(time_us, event_time_us) -
			                               min(time_us, event_time_us);
			Timeout const *timeout       = results[i].timeout;

			log(time_us / 1000, " ms: ", timeout->name, " timeout triggered,"
			    " error ", error_us, " us (max ", max_error_us, " us)");

			if (error_us > max_error_us) {
				error("absolute timeout error greater than ", max_error_us,
				      " us");
				error_cnt++;
			}
		}

		done.submit();
	}

	Mixed_timeouts(Env                       &env,
	               unsigned                  &error_cnt,
	               Signal_context_capability  done,
	               unsigned                   id)
	:
		Test(env, error_cnt, done, id, brief)
	{
		ot1.schedule(timeouts[3].us);
		ot2.schedule(timeouts[4].us);
		ot3.schedule(timeouts[5].us);
	}
};


struct Fast_polling : Test
{
	static constexpr char const *brief = "poll time pretty fast";

	enum { NR_OF_ROUNDS          = 4 };
	enum { MIN_ROUND_DURATION_MS = 2500 };
	enum { MIN_NR_OF_POLLS       = 1000 };
	enum { STACK_SIZE            = 4 * 1024 * sizeof(addr_t) };
	enum { MIN_TIME_COMPARISONS  = 100 };
	enum { MAX_TIME_ERR_US       = 10000 };
	enum { MAX_DELAY_ERR_US      = 2000 };
	enum { MAX_AVG_DELAY_ERR_US  = 20 };
	enum { MAX_POLL_LATENCY_US   = 1000 };

	struct Result_buffer
	{
		Attached_ram_dataspace  ram;
		uint64_t volatile      *value { ram.local_addr<uint64_t>() };

		Result_buffer(Env    &env,
		              size_t  size)
		:
			ram { env.ram(), env.rm(), size }
		{ }

		private:

			/*
			 * Noncopyable
			 */
			Result_buffer(Result_buffer const &);
			Result_buffer &operator = (Result_buffer const &);
	};

	Entrypoint                   main_ep;
	Signal_handler<Fast_polling> main_handler;

	Timer::Connection timer_2         { env };
	uint64_t const    timer_us        { timer.elapsed_us() };
	uint64_t const    timer_2_us      { timer_2.elapsed_us() };
	bool     const    timer_2_delayed { timer_us > timer_2_us };
	uint64_t const    timer_diff_us   { timer_2_delayed ?
	                                    timer_2_us - timer_us :
	                                    timer_us - timer_2_us };

	size_t const  buf_size        { config.xml().attribute_value("fast_polling_buf_size", (size_t)80000000) };
	size_t const  max_nr_of_polls { buf_size / sizeof(uint64_t) };
	Result_buffer local_us_1_buf  { env, buf_size };
	Result_buffer local_us_2_buf  { env, buf_size };
	Result_buffer remote_us_buf   { env, buf_size };

	uint64_t max_avg_time_err_us { config.xml().attribute_value("precise_ref_time", true) ?
	                               (uint64_t)1000 : (uint64_t)2000 };

	unsigned const delay_loops_per_poll[NR_OF_ROUNDS] {      1,
	                                                      1000,
	                                                     10000,
	                                                    100000  };

	/*
	 * Accumulates great amounts of integer values to one average value
	 *
	 * Aims for best possible precision with a fixed amount of integer buffers
	 */
	struct Average_accumulator
	{
		private:

			uint64_t _avg     { 0 };
			uint64_t _avg_cnt { 0 };
			uint64_t _acc     { 0 };
			uint64_t _acc_cnt { 0 };

		public:

			void flush()
			{
				uint64_t acc_avg = _acc / _acc_cnt;
				if (!_avg_cnt) { _avg = _avg + acc_avg; }
				else {
					float acc_fac = (float)_acc_cnt / _avg_cnt;
					_avg = (_avg + ((float)acc_fac * acc_avg)) / (1 + acc_fac);
				}
				_avg_cnt += _acc_cnt;
				_acc      = 0;
				_acc_cnt  = 0;
			}

			void add(uint64_t add)
			{
				if (add > (~(uint64_t)0 - _acc)) {
					flush(); }
				_acc += add;
				_acc_cnt++;
			}

			uint64_t avg()
			{
				if (_acc_cnt) {
					flush(); }
				return _avg;
			}

			uint64_t avg_cnt()
			{
				if (_acc_cnt) {
					flush(); }
				return _avg_cnt;
			}
	};

	uint64_t delay_us(unsigned poll)
	{
		return local_us_1_buf.value[poll - 1] > local_us_1_buf.value[poll] ?
		       local_us_1_buf.value[poll - 1] - local_us_1_buf.value[poll] :
		       local_us_1_buf.value[poll]     - local_us_1_buf.value[poll - 1];
	}

	unsigned long estimate_delay_loops_per_ms()
	{
		log("estimate CPU speed ...");
		for (unsigned long max_cnt = 1000UL * 1000UL; ; max_cnt *= 2) {

			/* measure consumed time of a limited busy loop */
			uint64_t volatile start_ms = timer_2.elapsed_ms();
			delay_loop(max_cnt);
			uint64_t volatile end_ms = timer_2.elapsed_ms();

			/*
			 * We only return the result if the loop was time intensive enough
			 * and therefore representative. Otherwise we raise the loop-
			 * counter limit and do a new estimation.
			 */
			uint64_t diff_ms = end_ms - start_ms;
			if (diff_ms > 1000) {
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
		 * few microseconds. Thus, to get nearly similar delays on each
		 * platform we have to do this estimation.
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

			unsigned long nr_of_polls           = max_nr_of_polls;
			unsigned long delay_loops_per_poll_ = delay_loops_per_poll[round];
			uint64_t end_remote_us              = timer_2.elapsed_us() +
			                                      MIN_ROUND_DURATION_MS * 1000;

			/* limit polling to our buffer capacity */
			for (unsigned poll = 0; poll < nr_of_polls; poll++) {

				/* create delay between two polls */
				delay_loop(delay_loops_per_poll_);

				/* count delay loops to limit frequency of remote time reading */
				delay_loops = delay_loops + delay_loops_per_poll_;

				/*
				 * We buffer the results in local variables first so the RAM
				 * access wont raise the delay between the reading of the
				 * different time values.
				 */
				uint64_t volatile local_us_1;
				uint64_t volatile local_us_2;
				uint64_t volatile remote_us;

				/* read local time before the remote time reading */
				local_us_1 = timer.curr_time().trunc_to_plain_us().value;

				/*
				 * Limit frequency of remote-time reading
				 *
				 * If we would stress the timer driver to much with the
				 * 'elapsed_us' method, the back-end functionality of the
				 * timeout framework would slow down too which causes a phase
				 * of adaption with bigger errors. But the goal of the
				 * framework is to spare calls to the timer driver anyway. So,
				 * its fine to limit the polling frequency here.
				 */
				if (delay_loops > delay_loops_per_remote_poll) {

					/* read remote time and second local time */
					remote_us  = timer_2.elapsed_us();
					local_us_2 = timer.curr_time().trunc_to_plain_us().value;

					/* reset delay counter for remote-time reading */
					delay_loops = 0;

				} else {

					/* mark remote-time and second local-time value invalid */
					remote_us  = 0;
					local_us_2 = 0;
				}
				/* store results to the buffers */
				remote_us_buf.value[poll]  = remote_us;
				local_us_1_buf.value[poll] = local_us_1;
				local_us_2_buf.value[poll] = local_us_2;

				/* if the minimum round duration is reached, end polling */
				if (remote_us > end_remote_us) {
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

				uint64_t const poll_latency_us =
					local_us_2_buf.value[poll] - local_us_1_buf.value[poll];

				if (remote_us_buf.value[poll] &&
				    poll_latency_us > MAX_POLL_LATENCY_US)
				{
					local_us_1_buf.value[poll] = 0;
					nr_of_bad_polls++;

				} else {

					if (timer_2_delayed) {
						local_us_1_buf.value[poll] =
							local_us_1_buf.value[poll] + timer_diff_us;
						local_us_2_buf.value[poll] =
							local_us_2_buf.value[poll] + timer_diff_us;
					} else {
						remote_us_buf.value[poll] =
							remote_us_buf.value[poll] + timer_diff_us;
					}
					nr_of_good_polls++;
				}
			}

			/*
			 * Calculate the average delay between consecutive polls
			 * (using the local time).
			 */
			Average_accumulator avg_delay_us;
			for (unsigned poll = 1; poll < nr_of_polls; poll++) {

				/* skip if this result was dismissed */
				if (!local_us_1_buf.value[poll]) {
					poll++;
					continue;
				}
				/* check if local time is monotone */
				if (local_us_1_buf.value[poll - 1] > local_us_1_buf.value[poll]) {

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
			uint64_t            max_time_err_us = 0;

			for (unsigned poll = 0; poll < nr_of_polls; poll++) {

				/* skip if this result was dismissed */
				if (!local_us_1_buf.value[poll]) {
					continue; }

				/* skip if this poll contains no remote time */
				if (!remote_us_buf.value[poll]) {
					continue; }

				uint64_t const remote_us   = remote_us_buf.value[poll];
				uint64_t const local_us    = local_us_1_buf.value[poll];
				uint64_t const time_err_us = remote_us > local_us ?
				                             remote_us - local_us :
				                             local_us - remote_us;

				/* update max time error */
				if (time_err_us > max_time_err_us) {
					max_time_err_us = time_err_us; }

				/* update average time error */
				avg_time_err_us.add(time_err_us);
			}
			Average_accumulator avg_delay_err_us;
			uint64_t avg_delay_us_ = avg_delay_us.avg();

			/*
			 * Calculate the average error of the delays compared to the
			 * average delay (in microseconds and percent of the average
			 * delay).
			 */
			uint64_t max_delay_err_us = 0;
			for (unsigned poll = 1; poll < nr_of_polls; poll++) {

				/* skip if this result was dismissed */
				if (!local_us_1_buf.value[poll]) {
					poll++;
					continue;
				}

				uint64_t delay_us_    = delay_us(poll);
				uint64_t delay_err_us = delay_us_ > avg_delay_us_ ?
				                        delay_us_ - avg_delay_us_ :
				                        avg_delay_us_ - delay_us_;

				if (delay_err_us > max_delay_err_us) {
					max_delay_err_us = delay_err_us; }

				avg_delay_err_us.add(delay_err_us);
			}

			uint64_t const max_avg_delay_err_us = (uint64_t)MAX_AVG_DELAY_ERR_US +
			                                      avg_delay_us_ / 20;

			bool const error_nr_of_good_polls = (nr_of_good_polls          < MIN_NR_OF_POLLS);
			bool const error_nr_of_time_cmprs = (avg_time_err_us.avg_cnt() < MIN_TIME_COMPARISONS);
			bool const error_avg_time_err     = (avg_time_err_us.avg()     > max_avg_time_err_us);
			bool const error_max_time_err     = (max_time_err_us           > MAX_TIME_ERR_US);
			bool const error_avg_delay_err    = (avg_delay_err_us.avg()    > max_avg_delay_err_us);

			error_cnt += error_nr_of_good_polls;
			error_cnt += error_nr_of_time_cmprs;
			error_cnt += error_avg_time_err;
			error_cnt += error_max_time_err;
			error_cnt += error_avg_delay_err;

			log(error_nr_of_good_polls ? "\033[31mbad:  " : "good: ", "nr of good polls       ", nr_of_good_polls,            " (min ", (unsigned)MIN_NR_OF_POLLS,         ")\033[0m");
			log(                                            "      ", "nr of bad polls        ", nr_of_bad_polls                                                                     );
			log(error_nr_of_time_cmprs ? "\033[31mbad:  " : "good: ", "nr of time comparisons ", avg_time_err_us.avg_cnt(),   " (min ", (unsigned)MIN_TIME_COMPARISONS,    ")\033[0m");
			log(error_avg_time_err     ? "\033[31mbad:  " : "good: ", "average time error     ", avg_time_err_us.avg(),    " us (max ", (uint64_t)max_avg_time_err_us,  " us)\033[0m");
			log(error_max_time_err     ? "\033[31mbad:  " : "good: ", "maximum time error     ", max_time_err_us,          " us (max ", (uint64_t)MAX_TIME_ERR_US,      " us)\033[0m");
			log(                                            "      ", "average delay          ", avg_delay_us.avg(),       " us"                                                     );
			log(error_avg_delay_err    ? "\033[31mbad:  " : "good: ", "average delay error    ", avg_delay_err_us.avg(),   " us (max ", max_avg_delay_err_us,           " us)\033[0m");
			log(                                            "      ", "maximum delay error    ", max_delay_err_us,         " us"                                                     );

		}
		done.submit();
	}

	Fast_polling(Env                       &env,
	             unsigned                  &error_cnt,
	             Signal_context_capability  done,
	             unsigned                   id)
	:
		Test(env, error_cnt, done, id, brief),
		main_ep(env, STACK_SIZE, "fast_polling_ep", Affinity::Location()),
		main_handler(main_ep, *this, &Fast_polling::main)
	{
		if (precise_time(config.xml())) {
			Signal_transmitter(main_handler).submit();
		} else {
			log("... skip test, requires the platform to support precise time");
			Test::done.submit();
		}
	}
};


struct Main
{
	Env                           &env;
	unsigned                       error_cnt   { 0 };
	Constructible<Lock_test>       test_0      { };
	Constructible<Duration_test>   test_1      { };
	Constructible<Fast_polling>    test_2      { };
	Constructible<Mixed_timeouts>  test_3      { };
	Signal_handler<Main>           test_0_done { env.ep(), *this, &Main::handle_test_0_done };
	Signal_handler<Main>           test_1_done { env.ep(), *this, &Main::handle_test_1_done };
	Signal_handler<Main>           test_2_done { env.ep(), *this, &Main::handle_test_2_done };
	Signal_handler<Main>           test_3_done { env.ep(), *this, &Main::handle_test_3_done };

	Main(Env &env) : env(env)
	{
		test_0.construct(env, error_cnt, test_0_done, 0);
	}

	void handle_test_0_done()
	{
		test_0.destruct();
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
		test_3.construct(env, error_cnt, test_3_done, 3);
	}

	void handle_test_3_done()
	{
		test_3.destruct();
		if (error_cnt) {
			error("test failed because of ", error_cnt, " error(s)");
			env.parent().exit(-1);
		} else {
			env.parent().exit(0);
		}
	}
};


void Component::construct(Env &env) { 
/*
Timer::Connection timer { env};
while (1) {
log(__func__,__LINE__);
timer.msleep(1000);
log(__func__,__LINE__);
}
*/
static Main main(env);
}
