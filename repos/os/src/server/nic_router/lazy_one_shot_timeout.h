/*
 * \brief  A wrapper for Timer::One_shot_timeout with lazy re-scheduling
 * \author Johannes Schlatow
 * \date   2022-07-01
 *
 * NOTE: This implementation is not thread safe and should only be used in
 *       single-threaded components.
 *
 * This implementation prevents re-scheduling when a timeout is frequently
 * updated with only marginal changes. Timeouts within a certain accuracy
 * threshold of the existing timeout will be ignored. Otherwise, earlier
 * timeouts will always be re-scheduled whereas later timeouts are never
 * applied immediately but only when the scheduled timeout occured.
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LAZY_ONE_SHOT_TIMEOUT_H_
#define _LAZY_ONE_SHOT_TIMEOUT_H_

/* local includes */
#include <cached_timer.h>

namespace Net {

	template <typename> class Lazy_one_shot_timeout;
}


template <typename HANDLER>
class Net::Lazy_one_shot_timeout
:
	private Timer::One_shot_timeout<Lazy_one_shot_timeout<HANDLER>>
{
	private:

		using One_shot_timeout = Timer::One_shot_timeout<Lazy_one_shot_timeout<HANDLER>>;
		using Microseconds     = Genode::Microseconds;
		using Duration         = Genode::Duration;
		using uint64_t         = Genode::uint64_t;
		using Handler_method   = void (HANDLER::*)(Duration);

		Cached_timer         &_timer;
		HANDLER              &_object;
		Handler_method const  _method;
		uint64_t       const  _tolerance_us;
		uint64_t              _postponed_deadline_us { 0 };

		void _handle_timeout(Duration curr_time)
		{
			_timer.cached_time(curr_time);

			/*
			 * If the postponed deadline is set and more than tolerance
			 * microseconds in the future, skip calling the user handler and
			 * re-schedule with the postponed deadline instead.
			 */
			if (_postponed_deadline_us > 0) {

				uint64_t const curr_time_us {
					curr_time.trunc_to_plain_us().value };

				if (curr_time_us + _tolerance_us < _postponed_deadline_us) {

					One_shot_timeout::schedule(
						Microseconds { _postponed_deadline_us - curr_time_us });

					_postponed_deadline_us = 0;
					return;
				}
			}
			/*
			 * If not, call the user handler.
			 */
			(_object.*_method)(curr_time);
		}

	public:

		using One_shot_timeout::discard;
		using One_shot_timeout::scheduled;

		Lazy_one_shot_timeout(Cached_timer   &timer,
		                      HANDLER        &object,
		                      Handler_method  method,
		                      Microseconds    tolerance)
		:
			One_shot_timeout { timer, *this,
			                   &Lazy_one_shot_timeout<HANDLER>::_handle_timeout },
			_timer           { timer },
			_object          { object },
			_method          { method },
			_tolerance_us    { tolerance.value }
		{ }

		/**
		 * In contrast to the original 'schedule' method, this wrapper evalutes
		 * whether scheduling must be done immediately, can be postponed to
		 * '_handle_timeout', or can even be skipped.
		 *
		 * Scheduling is done immediately if
		 *   the timeout is not active OR
		 *   new deadline < old deadline - tolerance
		 *
		 * Scheduling is postponed to '_handle_timeout' if
		 *   new deadline > old deadline + tolerance
		 *
		 * Scheduling is skipped if
		 *   new deadline >= old deadline - tolerance AND
		 *   new deadline <= old deadline + tolerance
		 */
		void schedule(Microseconds duration)
		{
			/* remove old postponed deadline */
			_postponed_deadline_us = 0;

			/* no special treatment if timeout is not scheduled */
			if (!scheduled()) {
				One_shot_timeout::schedule(duration);
				return;
			}
			uint64_t const curr_time_us {
				_timer.cached_time().trunc_to_plain_us().value };

			uint64_t const new_deadline_us {
				duration.value <= ~(uint64_t)0 - curr_time_us ?
					curr_time_us + duration.value : ~(uint64_t)0 };

			uint64_t const old_deadline_us { One_shot_timeout::deadline().value };

			if (new_deadline_us < old_deadline_us) {
				/*
				 * The new deadline is earlier. If the old deadline is not
				 * accurate enough, re-schedule. Else, drop the new deadline.
				 */
				if (new_deadline_us < old_deadline_us - _tolerance_us)
					One_shot_timeout::schedule(duration);

			} else {
				/*
				 * The new deadline is later. If the old deadline is not
				 * accurate enough, remember the new deadline and apply it in
				 * '_handle_timeout'. Else, drop the new deadline.
				 */
				if (new_deadline_us > old_deadline_us + _tolerance_us)
					_postponed_deadline_us = new_deadline_us;
			}
		}
};

#endif /* _LAZY_ONE_SHOT_TIMEOUT_H_ */
