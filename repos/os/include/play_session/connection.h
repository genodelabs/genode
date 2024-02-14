/*
 * \brief  Connection to audio-play service
 * \author Norman Feske
 * \date   2023-12-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PLAY_SESSION__CONNECTION_H_
#define _INCLUDE__PLAY_SESSION__CONNECTION_H_

#include <play_session/play_session.h>
#include <base/connection.h>
#include <base/attached_dataspace.h>
#include <base/sleep.h>

namespace Play { struct Connection; }


struct Play::Connection : Genode::Connection<Session>, Rpc_client<Session>
{
	private:

		static constexpr Ram_quota RAM_QUOTA = { DATASPACE_SIZE + 4096 };

		Attached_dataspace _ds;

		Shared_buffer &_buffer = *_ds.local_addr<Shared_buffer>();

		Seq _seq { };

		unsigned _slot_id { };

		Sample_start _sample_start { }; /* position within 'samples' ring buffer */

		Num_samples _submit_samples(auto const &fn)
		{
			_seq     = { _seq.value() + 1 };
			_slot_id = (_slot_id + 1) % Shared_buffer::NUM_SLOTS;

			Shared_buffer::Slot &slot = _buffer.slots[_slot_id];

			slot.acquired_seq = _seq;
			slot.time_window  = { };
			slot.sample_start = { };
			slot.num_samples  = { };

			class Submission
			{
				private:

					float * const _dst;
					unsigned      _pos;
					unsigned      _count = 0;

					/*
					 * Noncopyable
					 */
					Submission &operator = (Submission const &);
					Submission(Submission const &);

				public:

					Submission(float *dst, unsigned pos) :  _dst(dst), _pos(pos) { }

					void operator () (float value)
					{
						_dst[_pos] = value;
						if (++_pos >= Shared_buffer::MAX_SAMPLES)
							_pos = 0;
						_count++;
					}

					Num_samples num_samples() const { return { _count }; }
			};

			Submission submission(_buffer.samples, _sample_start.index);

			fn(submission);

			return submission.num_samples();
		}

		void _commit_to_current_slot(Num_samples n, Time_window tw)
		{
			Shared_buffer::Slot &slot = _buffer.slots[_slot_id];

			slot.sample_start  = _sample_start;
			slot.num_samples   = n;
			slot.time_window   = tw;
			slot.committed_seq = _seq;

			/* advance destination position for next submission */
			_sample_start.index = (_sample_start.index + n.value())
			                    % Shared_buffer::MAX_SAMPLES;
		}

	public:

		Connection(Genode::Env &env, Label const &label = Label())
		:
			Genode::Connection<Session>(env, label, RAM_QUOTA, Args()),
			Rpc_client<Session>(cap()),
			_ds(env.rm(), call<Rpc_dataspace>())
		{
			if (_ds.size() < DATASPACE_SIZE) {
				error("play buffer has insufficient size");
				sleep_forever();
			}
		}

		/**
		 * Schedule playback of data after the given 'previous' time window
		 *
		 * \param previous  time window returned by previous call, or
		 *                  a default-constructed 'Time_window' when starting
		 * \param duration  duration of the sample data in microseconds
		 * \param fn        functor to be repeatedly called with float sample values
		 *
		 * The sample rate depends on the given 'duration' and the number of
		 * 'fn' calls in the scope of the 'schedule' call. Note that the
		 * duration is evaluated only as a hint when starting a new playback.
		 * During continuous playback, the duration is inferred by the rate
		 * of the periodic 'schedule_and_enqueue' calls.
		 */
		Time_window schedule_and_enqueue(Time_window previous, Duration duration,
		                                 auto const &fn)
		{
			Num_samples const n  = _submit_samples(fn);
			Time_window const tw = call<Rpc_schedule>(previous, duration, n);

			_commit_to_current_slot(n, tw);

			return tw;
		}

		/**
		 * Passively enqueue data for the playback at the given time window
		 *
		 * In contrast to 'schedule_and_enqueue', this method does not allocate
		 * a new time window but schedules sample data for an already known
		 * time window. It is designated for the synchronized playback of
		 * multiple audio channels whereas each channel is a separate play
		 * session.
		 *
		 * One channel (e.g., the left) drives the allocation of time windows
		 * using 'schedule_and_enqueue' whereas the other channels (e.g., the
		 * right) merely submit audio data for the already allocated time
		 * windows using 'enqueue'. This way, only one RPC per period is needed
		 * to drive the synchronized output of an arbitrary number of channels.
		 */
		void enqueue(Time_window time_window, auto const &fn)
		{
			_commit_to_current_slot(_submit_samples(fn), time_window);
		}

		/**
		 * Inform the server that no further data is expected
		 *
		 * By calling 'stop', the client allows the server to distinguish the
		 * (temporary) end of playback from jitter.
		 */
		void stop() { call<Rpc_stop>(); }
};

#endif /* _INCLUDE__RECORD_SESSION__CONNECTION_H_ */
