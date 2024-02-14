/*
 * \brief  Record service of the audio mixer
 * \author Norman Feske
 * \date   2023-12-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _RECORD_SESSION_H_
#define _RECORD_SESSION_H_

/* Genode includes */
#include <root/component.h>
#include <base/session_object.h>
#include <record_session/record_session.h>

/* local includes */
#include <audio_signal.h>
#include <time_window_scheduler.h>

namespace Mixer {

	class Record_session;
	using Record_sessions = Registry<Record_session>;
	class Record_root;
}


class Mixer::Record_session : public Session_object<Record::Session, Record_session>,
                              private Registry<Record_session>::Element
{
	public:

		struct Operations : virtual Clock_operations
		{
			virtual void bind_sample_producers_to_record_sessions() = 0;
		};

	private:

		/*
		 * Noncopyable
		 */
		Record_session(Record_session const &);
		Record_session &operator = (Record_session const &);

		Attached_ram_dataspace    _ds;
		Signal_context_capability _wakeup_sigh { };

		using Scheduler = Time_window_scheduler;

		Scheduler::Config _config { };
		Scheduler _scheduler { };

		float _volume { };

		Time_window _previous { };

		Constructible<Clock> _stalled { };

		Sample_producer *_sample_producer_ptr = nullptr;

		Operations &_operations;

		bool _produce_scaled_sample_data(Time_window tw, Float_range_ptr &samples_ptr)
		{
			if (!_sample_producer_ptr)
				return false;

			if (!_sample_producer_ptr->produce_sample_data(tw, samples_ptr))
				return false;

			samples_ptr.scale(_volume);
			return true;
		}

	public:

		Record_session(Record_sessions &sessions,
		               Env             &env,
		               Resources const &resources,
		               Label     const &label,
		               Diag      const &diag,
		               Operations      &operations)
		:
			Session_object(env.ep(), resources, label, diag),
			Registry<Record_session>::Element(sessions, *this),
			_ds(env.ram(), env.rm(), Record::Session::DATASPACE_SIZE),
			_operations(operations)
		{
			_operations.bind_sample_producers_to_record_sessions();
		}

		void wakeup()
		{
			if (!_scheduler.consecutive() && _wakeup_sigh.valid())
				Signal_transmitter(_wakeup_sigh).submit();
		}

		void assign_sample_producer(Sample_producer &s) { _sample_producer_ptr = &s; }

		void release_sample_producer() { _sample_producer_ptr = nullptr; }

		void apply_config(Xml_node const &config, Scheduler::Config global)
		{
			_volume = float(config.attribute_value("volume", 1.0));

			_config = global;

			auto override_us_from_ms_attr = [&] (auto const &attr, auto &value)
			{
				if (config.has_attribute(attr))
					value = us_from_ms_attr(config, attr, 0.0);
			};

			override_us_from_ms_attr("period_ms", _config.period_us);
			override_us_from_ms_attr("jitter_ms", _config.jitter_us);
		}


		/******************************
		 ** Record session interface **
		 ******************************/

		Dataspace_capability dataspace() { return _ds.cap(); }

		void wakeup_sigh(Signal_context_capability sigh)
		{
			_wakeup_sigh = sigh;
			wakeup(); /* initial wakeup */
		}

		Record::Session::Record_result record(Record::Num_samples num_samples)
		{
			Clock const now = _operations.current_clock_value();

			_scheduler.track_activity({
				.time        = now,
				.num_samples = num_samples.value()
			});

			if (_scheduler.learned_jitter_ms() > _config.jitter_us/1000) {
				if (_operations.once_in_a_while()) {
					warning("jitter of ", _scheduler.learned_jitter_ms(), " ms is higher than expected");
					warning("(increase 'jitter_ms' attribute of record <policy> node?)");
				}
			}

			Time_window time_window { };

			_scheduler.record_window(_config, _previous).with_result(
				[&] (Time_window const &tw) {
					time_window = tw;
				},
				[&] (Scheduler::Record_window_error e) {
					Scheduler::Stats const stats = _scheduler.stats();
					unsigned const period_us = stats.median_period_us;

					switch (e) {

					case Scheduler::Record_window_error::JITTER_TOO_LARGE:

						if (_operations.once_in_a_while())
							warning("jitter too large for period of ",
							        float(period_us)/1000.0f, " ms");

						time_window = Time_window {
							.start = _previous.end,
							.end   = Clock{_previous.end}.after_us(1000).us()
						};
						break;

					case Scheduler::Record_window_error::INACTIVE:
						/* cannot happen because of 'track_activity' call above */
						error("attempt to allocate record window w/o activity");
					}
				}
			);

			Float_range_ptr samples_ptr(_ds.local_addr<float>(), num_samples.value());

			if (_produce_scaled_sample_data(time_window, samples_ptr)) {
				_stalled.destruct();

			} else {

				samples_ptr.clear();

				/* remember when samples become unavailable */
				if (!_stalled.constructed())
					_stalled.construct(now);

				/* tell the client to stop recording after some time w/o samples */
				if (now.later_than(_stalled->after_us(250*1000))) {
					_scheduler = { };
					_stalled.destruct();
					return Depleted();
				}
			}

			_previous = time_window;
			return time_window;
		}

		void record_at(Record::Time_window time_window, Record::Num_samples num_samples)
		{
			Float_range_ptr samples_ptr(_ds.local_addr<float>(), num_samples.value());

			if (!_produce_scaled_sample_data(time_window, samples_ptr))
				samples_ptr.clear();
		}
};


class Mixer::Record_root : public Root_component<Record_session>
{
	private:

		Env                        &_env;
		Record_sessions            &_sessions;
		Record_session::Operations &_operations;

	protected:

		Record_session *_create_session(const char *args) override
		{
			if (session_resources_from_args(args).ram_quota.value < Record::Session::DATASPACE_SIZE)
				throw Insufficient_ram_quota();

			return new (md_alloc())
				Record_session(_sessions,
				               _env,
				               session_resources_from_args(args),
				               session_label_from_args(args),
				               session_diag_from_args(args),
				               _operations);
		}

		void _upgrade_session(Record_session *s, const char *args) override
		{
			s->upgrade(ram_quota_from_args(args));
			s->upgrade(cap_quota_from_args(args));
		}

		void _destroy_session(Record_session *session) override
		{
			Genode::destroy(md_alloc(), session);
		}

	public:

		Record_root(Env &env, Allocator &md_alloc, Record_sessions &sessions,
		            Record_session::Operations &operations)
		:
			Root_component<Record_session>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _sessions(sessions), _operations(operations)
		{ }
};

#endif /* _RECORD_SESSION_H_ */
