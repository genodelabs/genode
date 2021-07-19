/*
 * \brief  Lx_kit timeout backend
 * \author Stefan Kalkowski
 * \date   2021-05-05
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__TIMEOUT_H_
#define _LX_KIT__TIMEOUT_H_

#include <timer_session/connection.h>

namespace Lx_kit {
	class Scheduler;
	class Timeout;

	using namespace Genode;
}


class Lx_kit::Timeout
{
	private:

		void _handle(Duration);

		using One_shot = Timer::One_shot_timeout<Timeout>;

		Scheduler         & _scheduler;
		One_shot            _timeout;

	public:

		void start(unsigned long us);
		void stop();

		Timeout(Timer::Connection & timer,
		        Scheduler & scheduler);
};

#endif /* _LX_KIT__TIMEOUT_H_ */
