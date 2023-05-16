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
#include <base/attached_rom_dataspace.h>
#include <timer_session/connection.h>

using namespace Genode;

class Measurement
{
	private:

		Env                                &_env;
		Signal_transmitter                  _done_transmitter;
		uint64_t                            _set_period_us;
		uint64_t                            _duration_us;
		unsigned long   volatile            _count                   { 0 };
		uint64_t        volatile            _elapsed_ms[3]           { 0, 0, 0 };
		uint64_t        volatile *volatile  _elapsed_ms_ptr          { _elapsed_ms };
		Timer::Connection                   _timer                   { _env };
		Signal_handler<Measurement>         _handler                 { _env.ep(), *this, &Measurement::_handle };
		uint64_t                            _nr_of_periods           { _duration_us / _set_period_us };
		double                              _error_pc                { 0 };
		double                              _avg_period_us           { 0 };
		uint64_t                            _elapsed_us              { 0 };

		void _handle()
		{
			_count = _count + 1;
			if (_count % _nr_of_periods != 1) {
				return;
			}
			*_elapsed_ms_ptr = _timer.elapsed_ms();
			_elapsed_ms_ptr = (uint64_t *)((addr_t)_elapsed_ms_ptr + sizeof(uint64_t));
			if (_count == 1) {
				return;
			}
			_timer.sigh(Signal_context_capability());

			_elapsed_us = (_elapsed_ms[1] - _elapsed_ms[0]) * 1'000;
			_avg_period_us = (double)_elapsed_us / (double)(_count - 1);
			_error_pc = ((double)_avg_period_us *
			          (double)100 / (double)_set_period_us) - 100;

			_done_transmitter.submit();
		}

		Measurement(const Measurement&);
		const Measurement& operator=(const Measurement&);

	public:

		Measurement(
			Env                       &env,
			Signal_context_capability  done_sigh,
			uint64_t                   set_period_us,
			uint64_t                   duration_us)
		:
			_env              { env },
			_done_transmitter { done_sigh },
			_set_period_us    { set_period_us },
			_duration_us      { duration_us }
		{
			log("  Measure: set period: ", _set_period_us,
			    " us, periods: ", _nr_of_periods);

			_timer.sigh(_handler);
			_timer.trigger_periodic(_set_period_us);
		}

		double error_pc() const { return _error_pc; }
		double avg_period_us() const { return _avg_period_us; }
		uint64_t elapsed_us() const { return _elapsed_us; }
};

class Test
{
	private:

		Env                           &_env;
		Signal_transmitter             _done_transmitter;
		Attached_rom_dataspace         _config_rom            { _env, "config" };
		uint64_t                       _max_abs_error_pc      { _config_rom.xml().attribute_value("max_abs_error_pc", (uint64_t)5) };
		uint64_t                       _measure_duration_us   { _config_rom.xml().attribute_value("measure_duration_us", (uint64_t)3'000'000) };
		uint64_t                       _min_good_bad_diff_us  { _config_rom.xml().attribute_value("min_good_bad_diff_us", (uint64_t)10) };
		uint64_t                       _good_period_us        { _measure_duration_us };
		uint64_t                       _bad_period_us         { 0 };
		uint64_t                       _set_period_us         { (_good_period_us - _bad_period_us) / 2 };
		Constructible<Measurement>     _measurement           { };
		Signal_handler<Test>           _measurement_done_sigh { _env.ep(), *this,
		                                                        &Test::_handle_measurement_done };

		void _handle_measurement_done()
		{
			if (_measurement.constructed()) {

				if (_measurement->error_pc() > (double)_max_abs_error_pc ||
				    _measurement->error_pc() < (double)_max_abs_error_pc * -1) {

					log("      Bad: avg period: ", _measurement->avg_period_us(),
					    " us, measure duration: ", _measurement->elapsed_us(),
					    " us, error: ", _measurement->error_pc(), " %");

					_bad_period_us = _set_period_us;

				} else {

					log("     Good: avg period: ", _measurement->avg_period_us(),
					    " us, measure duration: ", _measurement->elapsed_us(),
					    " us, error: ", _measurement->error_pc(), " %");

					_good_period_us = _set_period_us;
				}
				if (_good_period_us - _bad_period_us < _min_good_bad_diff_us) {

					log("Test result: lowest period value with error < ",
					    _max_abs_error_pc , "% is ", _good_period_us, " us");

					_done_transmitter.submit();
					return;
				}
				_set_period_us = _bad_period_us + ((_good_period_us - _bad_period_us) / 2);
			}
			_measurement.construct(_env, _measurement_done_sigh, _set_period_us, _measure_duration_us);
		}

	public:

		Test(Env                       &env,
		     Signal_context_capability  done_sigh)
		:
			_env              { env },
			_done_transmitter { done_sigh }
		{
			log("Test: find lowest period value with error < ",
			    _max_abs_error_pc, " %, measure duration: ",
			    _measure_duration_us, " us, min good-bad diff: ",
			    _min_good_bad_diff_us, " us");

			_handle_measurement_done();
		}
};

class Main
{
	private:

		Env                  &_env;
		Constructible<Test>   _test { };
		Signal_handler<Main>  _test_done_sigh { _env.ep(), *this, &Main::_handle_test_done };

		void _handle_test_done()
		{
			_env.parent().exit(0);
		}

	public:

		Main(Env &env)
		:
			_env { env }
		{
			_test.construct(_env, _test_done_sigh);
		}
};

void Component::construct(Env & env)
{
	static Main main { env };
}
