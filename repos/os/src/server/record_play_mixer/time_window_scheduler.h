/*
 * \brief  Jitter-aware time-window scheduler
 * \author Norman Feske
 * \date   2024-01-11
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TIME_WINDOW_SCHEDULER_H_
#define _TIME_WINDOW_SCHEDULER_H_

/* Genode includes */
#include <play_session/play_session.h>

/* local includes */
#include <types.h>
#include <median.h>

namespace Mixer { class Time_window_scheduler; }


class Mixer::Time_window_scheduler
{
	public:

		struct Entry
		{
			Clock    time;
			unsigned num_samples;
		};

		struct Config
		{
			unsigned period_us; /* period assumed before measurements are available */
			unsigned jitter_us; /* expected lower limit of jitter */
		};

		struct Stats
		{
			unsigned rate_hz;
			unsigned median_period_us;
			unsigned predicted_now_us;
			unsigned jitter_us;

			bool valid() const { return median_period_us > 0u; }

			void print(Output &out) const
			{
				Genode::print(out, "rate_hz=",      rate_hz,
				              " median_period_us=", median_period_us,
				              " predicted_now_us=", predicted_now_us,
				              " jitter_us=",        jitter_us);
			}
		};

	private:

		static constexpr unsigned N = 5;

		Entry _entries[N] { };

		unsigned _curr_index  = 0;
		unsigned _num_entries = 0;

		unsigned _learned_jitter_us = 5000;

		void _with_nth_entry(unsigned n, auto const &fn) const
		{
			unsigned const i = (_curr_index + N - n) % N;
			if (n < _num_entries)
				fn(_entries[i]);
		}

		void _for_each_period(auto const &fn) const
		{
			for (unsigned i = 0; i + 1 < _num_entries; i++)
				_with_nth_entry(i, [&] (Entry const &curr) {
					_with_nth_entry(i + 1, [&] (Entry const &prev) {
						fn(prev, curr); }); });
		}

		Stats _calc_stats() const
		{
			unsigned sum_period_us = 0,
			         num_periods   = 0;

			unsigned long sum_ages_us = 0, /* relative to latest */
			              sum_samples = 0;

			Entry latest { };
			_with_nth_entry(0, [&] (Entry const e) { latest = e; });

			Median<unsigned, N> median_period_us { };

			_for_each_period([&] (Entry const prev, Entry const curr) {

				unsigned const period_us = curr.time.us_since(prev.time);

				median_period_us.capture(period_us);

				num_periods++;
				sum_ages_us   += latest.time.us_since(prev.time);
				sum_period_us += period_us;
				sum_samples   += prev.num_samples;
			});

			if (num_periods == 0)
				return { };

			unsigned const avg_age_us = unsigned(sum_ages_us/num_periods);

			return {
				.rate_hz = (sum_period_us == 0) ? 0
				         : unsigned((sum_samples*1000*1000)/sum_period_us),

				.median_period_us = median_period_us.median(),

				.predicted_now_us = latest.time.before_us(avg_age_us)
				                               .after_us(sum_period_us/2).us(),

				.jitter_us = median_period_us.jitter(),
			};
		}

		void _learn_jitter(Stats const &stats)
		{
			_learned_jitter_us = (99*_learned_jitter_us)/100;

			if (stats.jitter_us > _learned_jitter_us)
				_learned_jitter_us = stats.jitter_us;
		}

		/*
		 * Compute delay on account to the (pre-)fetching the four probe
		 * values. Noticeable with extremely low sample rates and large
		 * periods.
		 */
		unsigned _prefetch_us(unsigned sample_rate_hz) const
		{
			if (sample_rate_hz == 0)
				return 0;

			unsigned const sample_distance_ns = (1000*1000*1000)/sample_rate_hz;

			return (4*sample_distance_ns)/1000;
		}

	public:

		Time_window_scheduler() { }

		void track_activity(Entry entry)
		{
			_curr_index = (_curr_index + 1) % N;
			_num_entries = min(_num_entries + 1, N);
			_entries[_curr_index] = entry;
		}

		bool consecutive() const { return (_num_entries > 1); }

		Stats stats() const { return _calc_stats(); }

		unsigned learned_jitter_ms() const { return _learned_jitter_us/1000; }

		enum class Play_window_error { INACTIVE, JITTER_TOO_LARGE };

		using Play_window_result = Attempt<Play::Time_window, Play_window_error>;

		Play_window_result play_window(Config            config,
		                               Play::Time_window previous,
		                               Play::Num_samples num_samples)
		{
			if (_num_entries == 0)
				return Play_window_error::INACTIVE;

			if (!consecutive()) {
				unsigned const rate_hz = num_samples.value()
				                       ? config.period_us/num_samples.value()
				                       : 0;
				unsigned const prefetch_us = _prefetch_us(rate_hz);
				Entry const now = _entries[_curr_index];
				return Play::Time_window  {
					.start = now.time.after_us(prefetch_us).us(),
					.end   = now.time.after_us(config.period_us + prefetch_us).us()
				};
			}

			Stats const stats = _calc_stats();

			_learn_jitter(stats);

			unsigned const jitter_us = max(_learned_jitter_us, config.jitter_us);
			unsigned const delay_us  = jitter_us + _prefetch_us(stats.rate_hz);

			Clock const start { previous.end },
			            end   { stats.predicted_now_us + stats.median_period_us + delay_us };

			if (Clock::range_valid(start, end))
				return Play::Time_window { start.us(), end.us() };

			return Play_window_error::JITTER_TOO_LARGE;
		}

		enum class Record_window_error { INACTIVE, JITTER_TOO_LARGE };

		using Record_window_result = Attempt<Record::Time_window, Record_window_error>;

		Record_window_result record_window(Config config, Record::Time_window previous)
		{
			if (_num_entries == 0)
				return Record_window_error::INACTIVE;

			if (!consecutive()) {
				Entry const now = _entries[_curr_index];
				return Record::Time_window  {
					.start = now.time.before_us(config.period_us
					                          + config.jitter_us).us(),
					.end   = now.time.us()
				};
			}

			Stats const stats = _calc_stats();

			_learn_jitter(stats);

			unsigned const jitter_us = max(_learned_jitter_us, config.jitter_us);

			Clock const start { previous.end },
			            end   { Clock{stats.predicted_now_us}.before_us(jitter_us).us() };

			if (Clock::range_valid(start, end))
				return Record::Time_window { start.us(), end.us() };

			return Record_window_error::JITTER_TOO_LARGE;
		}

		void print(Output &out) const
		{
			Stats const stats = _calc_stats();

			Genode::print(out, "now=",    _entries[_curr_index].time.us()/1000,
			              " (predicted ", stats.predicted_now_us/1000, ")"
			              " period=",     float(stats.median_period_us)/1000.0f,
			              " jitter=",     float(stats.jitter_us)/1000.0f,
			              " (learned ",   float(_learned_jitter_us)/1000.0f, ")",
			              " prefetch=",   float(_prefetch_us(stats.rate_hz))/1000.0f);
		}
};

#endif /* _TIME_WINDOW_SCHEDULER_H_ */
