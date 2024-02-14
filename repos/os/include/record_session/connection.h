/*
 * \brief  Connection to an audio-record service
 * \author Norman Feske
 * \date   2023-12-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RECORD_SESSION__CONNECTION_H_
#define _INCLUDE__RECORD_SESSION__CONNECTION_H_

#include <record_session/record_session.h>
#include <base/connection.h>
#include <base/attached_dataspace.h>
#include <base/sleep.h>

namespace Record { struct Connection; }


struct Record::Connection : Genode::Connection<Session>, Rpc_client<Session>
{
	private:

		Attached_dataspace _ds;

	public:

		static constexpr Ram_quota RAM_QUOTA = { DATASPACE_SIZE + 2*4096 };

		Connection(Env &env, Session_label const &label = Session_label())
		:
			Genode::Connection<Session>(env, label, RAM_QUOTA, Args()),
			Rpc_client<Session>(cap()),
			_ds(env.rm(), call<Rpc_dataspace>())
		{
			if (_ds.size() < DATASPACE_SIZE) {
				error("record buffer has insufficient size");
				sleep_forever();
			}
		}

		/**
		 * Register signal handler on new data becomes available after depletion
		 */
		void wakeup_sigh(Signal_context_capability sigh) { call<Rpc_wakeup_sigh>(sigh); }

		/**
		 * Read-only sample data as an array of float values
		 */
		struct Samples_ptr : Genode::Noncopyable
		{
			struct { float const * const start; unsigned num_samples; };

			Samples_ptr(float const *start, unsigned num_samples)
			: start(start), num_samples(num_samples) { }
		};

		/**
		 * Record the specified number of audio samples
		 *
		 * \param fn  called with the 'Time_window' and 'Samples_ptr const &'
		 *            of the recording
		 *
		 * \param depleted_fn  called when no sample data is available
		 *
		 * Subsequent 'record' calls result in consecutive time windows.
		 */
		void record(Num_samples n, auto const &fn, auto const &depleted_fn)
		{
			call<Rpc_record>(n).with_result(
				[&] (Time_window const &tw) {
					fn(tw, Samples_ptr(_ds.local_addr<float const>(), n.value()));
				},
				[&] (Depleted) { depleted_fn(); });
		}

		/**
		 * Record specified number of audio samples at the given time window
		 *
		 * By using the time window returned by 'record' as argument for
		 * 'record_at', a user of multiple sessions (e.g., for left and right)
		 * can obtain sample data synchronized between the sessions.
		 */
		void record_at(Time_window tw, Num_samples n, auto const &fn)
		{
			call<Rpc_record_at>(tw, n);
			fn(Samples_ptr(_ds.local_addr<float const>(), n.value()));
		}
};

#endif /* _INCLUDE__RECORD_SESSION__CONNECTION_H_ */
