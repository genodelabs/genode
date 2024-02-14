/*
 * \brief  Play service of the audio mixer
 * \author Norman Feske
 * \date   2023-12-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PLAY_SESSION_H_
#define _PLAY_SESSION_H_

/* Genode includes */
#include <util/formatted_output.h>
#include <root/component.h>
#include <base/session_object.h>
#include <play_session/play_session.h>

/* local includes */
#include <types.h>
#include <time_window_scheduler.h>

namespace Mixer { class Play_root; }


class Mixer::Play_session : public Session_object<Play::Session, Play_session>,
                            public Sample_producer,
                            private Registry<Play_session>::Element
{
	public:

		struct Operations : virtual Clock_operations
		{
			virtual void bind_play_sessions_to_audio_signals() = 0;
			virtual void wakeup_record_clients() = 0;
		};

	private:

		Attached_ram_dataspace _ds;

		Shared_buffer const &_buffer = *_ds.local_addr<Shared_buffer>();

		Operations &_operations;

		Play::Seq _latest_seq  { };
		Play::Seq _stopped_seq { }; /* latest seq number at stop time */

		bool _stopped() const { return _latest_seq.value() == _stopped_seq.value(); }

		using Scheduler = Time_window_scheduler;

		Scheduler _scheduler { };

		unsigned _expected_jitter_us = 0;

		/*
		 * Cached meta data fetched from shared buffer
		 */
		struct Slot
		{
			Clock     start, end;
			unsigned  sample_start;
			unsigned  num_samples;
			Play::Seq seq;

			unsigned duration_us = end.us_since(start);

			bool _valid() const { return duration_us > 0 && num_samples > 1; }

			unsigned dt() const
			{
				return _valid() ? duration_us / num_samples : 0;
			}

			bool contains(Clock t) const
			{
				return _valid() && !start.later_than(t)
				                &&  t.earlier_than(end);
			}

			void with_index_for_t(Clock t, auto const &fn) const
			{
				if (!contains(t))
					return;

				unsigned const rel_t = t.us_since(start);
				unsigned const index = (rel_t*num_samples) / duration_us;

				fn(index);
			}

			void with_time_window_at_index(unsigned i, auto const &fn) const
			{
				if (i >= num_samples || !_valid())
					return;

				Clock const t_start = start.after_us((i*duration_us) / num_samples);

				fn(t_start, dt());
			}

			float u_for_t(unsigned index, Clock t) const
			{
				float result = 0.5f;
				with_time_window_at_index(index, [&] (Clock const t_start, unsigned dt) {
					result = float(t.us_since(t_start))/float(dt); });

				auto clamped = [] (float v) { return min(1.0f, max(0.0f, v)); };

				return clamped(result);
			}

			void print(Output &out) const
			{
				Genode::print(out, Time_window { start.us(), end.us() }, " seq=", seq.value());
			}
		};

		Slot _slots[Shared_buffer::NUM_SLOTS] { };

		/**
		 * Coordinate of a sample within the shared buffer
		 */
		struct Position
		{
			unsigned slot_id;
			unsigned index;    /* relative to the slot's 'sample_start' */

			Position next(Play_session const &session) const
			{
				if (index + 1 < session._slots[slot_id].num_samples)
					return {
						.slot_id = slot_id,
						.index   = index + 1,
					};

				/* proceed to next slot */
				return {
					.slot_id = (slot_id + 1) % Shared_buffer::NUM_SLOTS,
					.index   = 0,
				};
			}
		};

		struct Probe
		{
			float v[4] { };
			float u = 0.0f;

			Probe(Play_session const &session, Position pos, Clock t)
			{
				/*
				 * Technically, the 'u' value ought to be computed between t1
				 * and t2 (not between t0 and t1). Since the sample values are
				 * taken in steps of dt, the u values are the same except when
				 * dt is not constant (when crossing slot boundaries). However,
				 * even in this case, u_01 approximates u_12.
				 */
				u = session._slots[pos.slot_id].u_for_t(pos.index, t);

				for (unsigned i = 0; i < 4; i++, pos = pos.next(session)) {
					unsigned const index = session._slots[pos.slot_id].sample_start + pos.index;
					v[i] = session._buffer.samples[index % Shared_buffer::MAX_SAMPLES];
				}
			}

			void print(Output &out) const
			{
				Genode::print(out, " ", v[0], " ", v[1], "   (u:", Right_aligned(6, u), ")"
				                   " ", v[2], " ", v[3]);
			}
		};

		enum class Probe_result { OK, MISSING, AMBIGUOUS };

		Probe_result _with_start_position_at(Clock t, auto fn) const
		{
			Position pos { };

			unsigned matching_slots = 0;

			for (unsigned slot_id = 0; slot_id < Shared_buffer::NUM_SLOTS; slot_id++) {
				_slots[slot_id].with_index_for_t(t, [&] (unsigned index) {
					matching_slots++;
					pos = Position { .slot_id = slot_id,
					                 .index   = index, }; }); }

			if (matching_slots == 1u) {
				fn(pos);
				return Probe_result::OK;
			}

			if (matching_slots == 0u)
				return Probe_result::MISSING;

			return Probe_result::AMBIGUOUS;
		}

		Probe_result _with_interpolated_sample_value(Clock const t, auto const &fn) const
		{
			return _with_start_position_at(t, [&] (Position pos) {

				Probe probe(*this, pos, t);

				/* b-spline blending functions (u and v denote position v1 <-> v2) */
				float const u  = probe.u, v = 1.0f - u,
				            uu = u*u, uuu = u*uu,
				            vv = v*v, vvv = v*vv;

				float const b0 = vvv/6.0f,
				            b1 = uuu/2.0f - uu + 4.0f/6.0f,
				            b2 = vvv/2.0f - vv + 4.0f/6.0f,
				            b3 = uuu/6.0f;

				float const avg = b0*probe.v[0] + b1*probe.v[1] + b2*probe.v[2] + b3*probe.v[3];

				fn(avg);
			});
		}

	public:

		Play_session(Play_sessions   &sessions,
		             Env             &env,
		             Resources const &resources,
		             Label     const &label,
		             Diag      const &diag,
		             Operations      &operations)
		:
			Session_object(env.ep(), resources, label, diag),
			Registry<Play_session>::Element(sessions, *this),
			_ds(env.ram(), env.rm(), Play::Session::DATASPACE_SIZE),
			_operations(operations)
		{
			_operations.bind_play_sessions_to_audio_signals();
		}

		void global_jitter_us(unsigned us) { _expected_jitter_us = us; };

		void expect_jitter_us(unsigned us)
		{
			_expected_jitter_us = max(_expected_jitter_us, us);
		}

		/**
		 * Sample_producer interface
		 */
		bool produce_sample_data(Time_window tw, Float_range_ptr &samples) override
		{
			bool result = false;

			/*
			 * Make local copy of meta data from shared buffer to ensure
			 * operating on consistent values during 'produce_sample_data'.
			 * Import slot meta data only if not currently modified by the
			 * client.
			 */
			for (unsigned i = 0; i < Shared_buffer::NUM_SLOTS; i++) {

				Play::Session::Shared_buffer::Slot const &src = _buffer.slots[i];
				Slot                                     &dst = _slots[i];

				dst = { };

				Play::Seq const acquired_seq = src.acquired_seq;

				Slot const slot { .start        = Clock { src.time_window.start },
				                  .end          = Clock { src.time_window.end   },
				                  .sample_start = src.sample_start.index,
				                  .num_samples  = src.num_samples.value(),
				                  .seq          = src.acquired_seq };

				Play::Seq const committed_seq = src.committed_seq;

				if (acquired_seq.value() == committed_seq.value()) {
					dst = slot;

					if (_latest_seq < slot.seq)
						_latest_seq = slot.seq;
				}
			}

			auto anything_scheduled = [&]
			{
				for (unsigned i = 0; i < Shared_buffer::NUM_SLOTS; i++)
					if (_slots[i].num_samples)
						return true;
				return false;
			};

			if (!anything_scheduled())
				return false;

			for_each_sub_window<1>(tw, samples, [&] (Time_window sub_tw, Float_range_ptr &dst) {

				Clock const t { sub_tw.start };

				auto probe_result = _with_interpolated_sample_value(t,
					[&] (float v) {
						dst.start[0] = v;
						result = true; });

				if (probe_result == Probe_result::OK)
					return;

				if (_operations.once_in_a_while()) {

					bool earlier_than_avail_samples = false,
					     later_than_avail_samples   = false;

					for (unsigned i = 0; i < Shared_buffer::NUM_SLOTS; i++) {
						if (_slots[i].duration_us) {
							if (t.earlier_than(_slots[i].start))
								earlier_than_avail_samples = true;

							if (_slots[i].end.earlier_than(t))
								later_than_avail_samples = true;
						}
					}

					if (probe_result == Probe_result::MISSING) {
						if (earlier_than_avail_samples) {
							warning("required sample value is no longer available");
							warning("(jitter config or period too high?)");
						}
						else if (later_than_avail_samples && !_stopped()) {
							warning("required sample is not yet available");
							warning("(increase 'jitter_ms' config attribute?)");
						}
					}

					if (probe_result == Probe_result::AMBIGUOUS)
						warning("ambiguous sample value for t=", float(t.us())/1000);
				}
			});

			return result;
		}


		/****************************
		 ** Play session interface **
		 ****************************/

		Dataspace_capability dataspace() { return _ds.cap(); }

		Play::Time_window schedule(Play::Time_window previous,
		                           Play::Duration    duration,
		                           Play::Num_samples num_samples)
		{
			using Time_window = Play::Time_window;

			if (!duration.valid() || num_samples.value() == 0)
				return { };

			/* playback just started, reset scheduler */
			if (previous.start == previous.end)
				_scheduler = { };

			_scheduler.track_activity({
				.time        = _operations.current_clock_value(),
				.num_samples = num_samples.value()
			});

			if (_scheduler.learned_jitter_ms() > _expected_jitter_us/1000) {
				if (_operations.once_in_a_while()) {
					warning("jitter of ", _scheduler.learned_jitter_ms(), " ms is higher than expected");
					warning("(increase 'jitter_ms' attribute of <play> node?)");
				}
			}

			Scheduler::Config const config {
				.period_us = duration.us,
				.jitter_us = _expected_jitter_us
			};

			Scheduler::Play_window_result const window =
				_scheduler.play_window(config, previous, num_samples);

			if (!_scheduler.consecutive())
				_operations.wakeup_record_clients();

			return window.convert<Time_window>(
				[&] (Time_window const &tw) -> Time_window { return tw; },
				[&] (Scheduler::Play_window_error e) -> Time_window {
					Scheduler::Stats const stats = _scheduler.stats();
					unsigned const period_us = stats.median_period_us;

					switch (e) {

					case Scheduler::Play_window_error::JITTER_TOO_LARGE:

						if (_operations.once_in_a_while())
							warning("jitter too large for period of ",
							        float(period_us)/1000.0f, " ms");

						return {
							.start = previous.end,
							.end   = Clock{previous.end}.after_us(1000).us()
						};

					case Scheduler::Play_window_error::INACTIVE:
						/* cannot happen because of 'track_activity' call above */
						error("attempt to allocate play window w/o activity");
					}
					return  { };
				}
			);
		}

		void stop()
		{
			/* remember latest seq number at stop time */
			for (unsigned i = 0; i < Shared_buffer::NUM_SLOTS; i++)
				if (_latest_seq < _buffer.slots[i].committed_seq)
					_latest_seq = _buffer.slots[i].committed_seq;

			_stopped_seq = _latest_seq;

			/* discard period-tracking state */
			_scheduler = { };
		}
};


class Mixer::Play_root : public Root_component<Play_session>
{
	private:

		Env                      &_env;
		Play_sessions            &_sessions;
		Play_session::Operations &_operations;

	protected:

		Play_session *_create_session(const char *args) override
		{
			if (session_resources_from_args(args).ram_quota.value < Play::Session::DATASPACE_SIZE)
				throw Insufficient_ram_quota();

			return new (md_alloc())
				Play_session(_sessions,
				             _env,
				             session_resources_from_args(args),
				             session_label_from_args(args),
				             session_diag_from_args(args),
				             _operations);
		}

		void _upgrade_session(Play_session *s, const char *args) override
		{
			s->upgrade(ram_quota_from_args(args));
			s->upgrade(cap_quota_from_args(args));
		}

		void _destroy_session(Play_session *session) override
		{
			Genode::destroy(md_alloc(), session);
			_operations.bind_play_sessions_to_audio_signals();
		}

	public:

		Play_root(Env &env, Allocator &md_alloc, Play_sessions &sessions,
		          Play_session::Operations &operations)
		:
			Root_component<Play_session>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _sessions(sessions), _operations(operations)
		{ }
};

#endif /* _PLAY_SESSION_H_ */
