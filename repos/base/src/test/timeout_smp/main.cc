/*
 * \brief  Test multiprocessor support of Timeout framework
 * \author Martin Stein
 * \date   2020-09-10
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License ve sion 3.
 */

/* Genode includes */
#include <base/component.h>
#include <timer_session/connection.h>

using namespace Genode;

enum { MIN_NR_OF_TEST_ITERATIONS = 10 };

template <typename TYPE>
class Test_thread
:
	public Thread,
	public Noncopyable
{
	private:

		enum { STACK_SIZE = sizeof(unsigned long) * 4096 };

		typedef void (TYPE::*Method)();

		TYPE         &_object;
		Method const  _method;

		void entry() override { (_object.*_method)(); }

	public:

		Test_thread(
			Env             &env,
			TYPE            &object,
			Method           method,
			int              cpu_idx,
			Affinity::Space  affinity_space)
		:
			Thread {
				env,
				Name("test_thread"),
				STACK_SIZE,
				affinity_space.location_of_index(
					cpu_idx % affinity_space.total()),
				Weight(),
				env.cpu()
			},
			_object { object },
			_method  { method }
		{ }
};

template <unsigned NR_OF_TIMEOUTS>
class Test_smp_2
{
	private:

		using Test_timeout = Timer::One_shot_timeout<Test_smp_2>;

		Env                &_env;
		unsigned long      &_nr_of_errors;
		Timer::Connection   _timeout_timer      { _env };
		Timer::Connection   _sleep_timer        { _env };
		Test_timeout        _timeout_1          { _timeout_timer, *this, &Test_smp_2::_handle_timeout_1 };
		Test_timeout        _timeout_2          { _timeout_timer, *this, &Test_smp_2::_handle_timeout_2 };
		Test_timeout        _timeout_3          { _timeout_timer, *this, &Test_smp_2::_handle_timeout_3 };
		Test_timeout        _timeout_4          { _timeout_timer, *this, &Test_smp_2::_handle_timeout_4 };
		Test_timeout        _timeout_5          { _timeout_timer, *this, &Test_smp_2::_handle_timeout_5 };
		unsigned long       _count_1            { 0 };
		unsigned long       _count_2            { 0 };
		unsigned long       _count_3            { 0 };
		unsigned long       _count_4            { 0 };
		unsigned long       _count_5            { 0 };
		int                 _cpu_idx            { 1 };
		bool volatile       _timeouts_discarded { false };
		bool                _done_called        { false };
		Mutex               _done_mutex         { };
		Signal_transmitter  _done_transmitter;
		Affinity::Space     _affinity_space     { _env.cpu().affinity_space() };

		Test_thread<Test_smp_2> _discard_timeouts_thread
		{
			_env, *this, &Test_smp_2::discard_timeouts_thread_entry,
			_cpu_idx++, _affinity_space
		};

		void inline _handle_timeout(Test_timeout  &timeout,
		                            unsigned long &count)
		{
			if (_timeouts_discarded) {
				Mutex::Guard guard { _done_mutex };
				log("  Timeout handler called after timeouts were discarded");
				_done(false);
			} else {
				count++;
				timeout.schedule(Microseconds(1));
			}
		}

		void _handle_timeout_1(Duration) { _handle_timeout(_timeout_1, _count_1); }
		void _handle_timeout_2(Duration) { _handle_timeout(_timeout_2, _count_2); }
		void _handle_timeout_3(Duration) { _handle_timeout(_timeout_3, _count_3); }
		void _handle_timeout_4(Duration) { _handle_timeout(_timeout_4, _count_4); }
		void _handle_timeout_5(Duration) { _handle_timeout(_timeout_5, _count_5); }

		void _done(bool success)
		{
			if (_done_called) {
				return;
			}
			_done_called = true;

			if (NR_OF_TIMEOUTS >= 1 && _count_1 < MIN_NR_OF_TEST_ITERATIONS) {
				log("  Timeout 1 has to be handled at least ",
				    (unsigned)MIN_NR_OF_TEST_ITERATIONS, " times");
				success = false;
			}
			if (NR_OF_TIMEOUTS >= 2 && _count_2 < MIN_NR_OF_TEST_ITERATIONS) {
				log("  Timeout 2 has to be handled at least ",
				    (unsigned)MIN_NR_OF_TEST_ITERATIONS, " times");
				success = false;
			}
			if (NR_OF_TIMEOUTS >= 3 && _count_3 < MIN_NR_OF_TEST_ITERATIONS) {
				log("  Timeout 3 has to be handled at least ",
				    (unsigned)MIN_NR_OF_TEST_ITERATIONS, " times");
				success = false;
			}
			if (NR_OF_TIMEOUTS >= 4 && _count_4 < MIN_NR_OF_TEST_ITERATIONS) {
				log("  Timeout 4 has to be handled at least ",
				    (unsigned)MIN_NR_OF_TEST_ITERATIONS, " times");
				success = false;
			}
			if (NR_OF_TIMEOUTS >= 5 && _count_5 < MIN_NR_OF_TEST_ITERATIONS) {
				log("  Timeout 5 has to be handled at least ",
				    (unsigned)MIN_NR_OF_TEST_ITERATIONS, " times");
				success = false;
			}
			if (success) {
				log("  Succeeded");
			} else {
				log("  Failed");
				_nr_of_errors++;
			}
			if (NR_OF_TIMEOUTS >= 1) { log("    Timeout 1 handled: ", _count_1, " times"); }
			if (NR_OF_TIMEOUTS >= 2) { log("    Timeout 2 handled: ", _count_2, " times"); }
			if (NR_OF_TIMEOUTS >= 3) { log("    Timeout 3 handled: ", _count_3, " times"); }
			if (NR_OF_TIMEOUTS >= 4) { log("    Timeout 4 handled: ", _count_4, " times"); }
			if (NR_OF_TIMEOUTS >= 5) { log("    Timeout 5 handled: ", _count_5, " times"); }
			_done_transmitter.submit();
		}

	public:

		Test_smp_2(
			Env                       &env,
			unsigned long             &nr_of_errors,
			Signal_context_capability  done_sigh,
			unsigned long              test_idx)
		:
			_env { env },
			_nr_of_errors { nr_of_errors },
			_done_transmitter { done_sigh }
		{
			log("Start test ", test_idx);
			_discard_timeouts_thread.start();
			if (NR_OF_TIMEOUTS >= 1) { _timeout_1.schedule(Microseconds(1)); }
			if (NR_OF_TIMEOUTS >= 2) { _timeout_2.schedule(Microseconds(1)); }
			if (NR_OF_TIMEOUTS >= 3) { _timeout_3.schedule(Microseconds(1)); }
			if (NR_OF_TIMEOUTS >= 4) { _timeout_4.schedule(Microseconds(1)); }
			if (NR_OF_TIMEOUTS >= 5) { _timeout_5.schedule(Microseconds(1)); }
		}

		~Test_smp_2()
		{
			_discard_timeouts_thread.join();
		}

		void discard_timeouts_thread_entry()
		{
			Timer::Connection sleep_timer { _env };
			sleep_timer.msleep(500);
			if (NR_OF_TIMEOUTS >= 1) { _timeout_1.discard(); }
			if (NR_OF_TIMEOUTS >= 2) { _timeout_2.discard(); }
			if (NR_OF_TIMEOUTS >= 3) { _timeout_3.discard(); }
			if (NR_OF_TIMEOUTS >= 4) { _timeout_4.discard(); }
			if (NR_OF_TIMEOUTS >= 5) { _timeout_5.discard(); }
			_timeouts_discarded = true;
			sleep_timer.msleep(500);

			Mutex::Guard guard { _done_mutex };
			_done(true);
		}
};


class Test_smp_1
{
	private:

		using Test_timeout =
			Constructible<Timer::One_shot_timeout<Test_smp_1> >;

		Env                      &_env;
		unsigned long            &_nr_of_errors;
		int                       _cpu_idx { 1 };
		bool                      _max_nr_of_handle_calls_reached { false };
		Timer::Connection         _timeout_timer { _env };
		Timer::Connection         _sleep_timer { _env };
		Timer::Connection         _cancel_test_thread_timer { _env };
		Test_timeout              _timeout { };
		bool                      _done_called { false };
		Mutex                     _done_mutex { };
		Signal_transmitter        _done_transmitter;
		Affinity::Space           _affinity_space { _env.cpu().affinity_space() };
		unsigned long             _nr_of_handle_calls { 0 };
		unsigned long             _nr_of_discard_calls { 0 };
		unsigned long             _nr_of_destruct_calls { 0 };

		Test_thread<Test_smp_1> _destruct_discard_timeout_thread
		{
			_env, *this, &Test_smp_1::destruct_discard_timeout_thread_entry,
			_cpu_idx++, _affinity_space
		};

		Test_thread<Test_smp_1> _cancel_test_thread
		{
			_env, *this, &Test_smp_1::cancel_test_thread_entry, _cpu_idx++,
			_affinity_space
		};

		void _handle_timeout(Duration)
		{
			if (_nr_of_handle_calls < 1000) {
				_nr_of_handle_calls++;
				_schedule_timeout();
			} else {
				_max_nr_of_handle_calls_reached = true;
			}
		}

		void _schedule_timeout()
		{
			_timeout->schedule(Microseconds(333));
		}

		void _construct_timeout()
		{
			_timeout.construct(
				_timeout_timer,
				*this,
				&Test_smp_1::_handle_timeout
			);
			_schedule_timeout();
		}

		void _done(bool success)
		{
			if (_done_called) {
				return;
			}
			_done_called = true;

			if (_nr_of_handle_calls < MIN_NR_OF_TEST_ITERATIONS) {
				log("  Timeout has to be handled at least ",
				    (unsigned)MIN_NR_OF_TEST_ITERATIONS, " times");
				success = false;
			}
			if (_nr_of_discard_calls < MIN_NR_OF_TEST_ITERATIONS) {
				log("  Timeout has to be discarded at least ",
				    (unsigned)MIN_NR_OF_TEST_ITERATIONS, " times");
				success = false;
			}
			if (_nr_of_destruct_calls < MIN_NR_OF_TEST_ITERATIONS) {
				log("  Timeout has to be destructed at least ",
				    (unsigned)MIN_NR_OF_TEST_ITERATIONS, " times");
				success = false;
			}
			if (success) {
				log("  Succeeded");
			} else {
				log("  Failed");
				_nr_of_errors++;
			}
			log("    Handled: ", _nr_of_handle_calls, " times");
			log("    Discarded: ", _nr_of_discard_calls, " times");
			log("    Destructed: ", _nr_of_destruct_calls, " times");
			_done_transmitter.submit();
		}

	public:

		Test_smp_1(
			Env                       &env,
			unsigned long             &nr_of_errors,
			Signal_context_capability  done_sigh,
			unsigned long              test_idx)
		:
			_env { env },
			_nr_of_errors { nr_of_errors },
			_done_transmitter { done_sigh }
		{
			log("Start test ", test_idx);
			_construct_timeout();
			_cancel_test_thread.start();
			_destruct_discard_timeout_thread.start();
		}

		~Test_smp_1()
		{
			_destruct_discard_timeout_thread.join();
			_cancel_test_thread.join();
		}

		void destruct_discard_timeout_thread_entry()
		{
			Timer::Connection sleep_timer { _env };
			while (true) {
				if (_max_nr_of_handle_calls_reached) {
					Mutex::Guard guard { _done_mutex };
					_done(true);
					break;
				}
				if (_nr_of_destruct_calls < _nr_of_discard_calls) {
					sleep_timer.msleep(25);
					_timeout.destruct();
					sleep_timer.msleep(9);
					_nr_of_destruct_calls++;
					_construct_timeout();
				} else {
					sleep_timer.msleep(23);
					_timeout->discard();
					sleep_timer.msleep(11);
					_nr_of_discard_calls++;
					_schedule_timeout();
				}
			}
		}

		void cancel_test_thread_entry()
		{
			Timer::Connection sleep_timer { _env };
			for (unsigned seconds = 0; seconds < 30; seconds++) {
				sleep_timer.msleep(1000);
				if (_done_called) {
					return;
				}
			}
			Mutex::Guard guard { _done_mutex };
			log("  Test didn't finish in time");
			_done(false);
		}
};

class Main
{
	private:

		Env           &_env;
		unsigned long  _nr_of_errors { 0 };

		Constructible<Test_smp_2<1> > _test_0 { };
		Constructible<Test_smp_2<2> > _test_1 { };
		Constructible<Test_smp_2<3> > _test_2 { };
		Constructible<Test_smp_2<4> > _test_3 { };
		Constructible<Test_smp_2<5> > _test_4 { };
		Constructible<Test_smp_1>     _test_5 { };

		Signal_handler<Main> _test_0_done_sigh { _env.ep(), *this, &Main::_handle_test_0_done };
		Signal_handler<Main> _test_1_done_sigh { _env.ep(), *this, &Main::_handle_test_1_done };
		Signal_handler<Main> _test_2_done_sigh { _env.ep(), *this, &Main::_handle_test_2_done };
		Signal_handler<Main> _test_3_done_sigh { _env.ep(), *this, &Main::_handle_test_3_done };
		Signal_handler<Main> _test_4_done_sigh { _env.ep(), *this, &Main::_handle_test_4_done };
		Signal_handler<Main> _test_5_done_sigh { _env.ep(), *this, &Main::_handle_test_5_done };

		void _handle_test_0_done() { _test_0.destruct(); _test_1.construct(_env, _nr_of_errors, _test_1_done_sigh, 1); }
		void _handle_test_1_done() { _test_1.destruct(); _test_2.construct(_env, _nr_of_errors, _test_2_done_sigh, 2); }
		void _handle_test_2_done() { _test_2.destruct(); _test_3.construct(_env, _nr_of_errors, _test_3_done_sigh, 3); }
		void _handle_test_3_done() { _test_3.destruct(); _test_4.construct(_env, _nr_of_errors, _test_4_done_sigh, 4); }
		void _handle_test_4_done() { _test_4.destruct(); _test_5.construct(_env, _nr_of_errors, _test_5_done_sigh, 5); }
		void _handle_test_5_done() { _test_5.destruct();
			if (_nr_of_errors > 0) {
				log("Some tests failed");
				_env.parent().exit(-1);
			} else {
				log("All tests succeeded");
				_env.parent().exit(0);
			}
		}

	public:

		Main(Env &env)
		:
			_env { env }
		{
			_test_0.construct(_env, _nr_of_errors, _test_0_done_sigh, 0);
		}
};

void Component::construct(Env & env)
{
	static Main main { env };
}
