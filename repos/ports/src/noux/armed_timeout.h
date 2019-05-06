/*
 * \brief  Timeout mechanism for 'select'
 * \author Norman Feske
 * \date   2011-02-14
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__ARMED_TIMEOUT_H_
#define _NOUX__ARMED_TIMEOUT_H_

#include <timer_session/connection.h>

namespace Noux { class Armed_timeout; }


class Noux::Armed_timeout : Noncopyable
{
	public:

		struct State : Noncopyable
		{
			State() { }
			bool timed_out { };
		};

	private:

		State &_state;
		Lock  &_blocker;

		Timer::One_shot_timeout<Armed_timeout> _one_shot_timeout;

		void _handle_one_shot_timeout(Duration)
		{
			_state.timed_out = true;
			_blocker.unlock();
		}

	public:

		Armed_timeout(State &state, Lock &blocker,
		              Timer::Connection &timer, Microseconds microseconds)
		:
			_state(state), _blocker(blocker),
			_one_shot_timeout(timer, *this, &Armed_timeout::_handle_one_shot_timeout)
		{
			_state.timed_out = false;
			_one_shot_timeout.schedule(microseconds);
		}

		void discard() { _one_shot_timeout.discard(); }
};

#endif /* _NOUX__ARMED_TIMEOUT_H_ */
