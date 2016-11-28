/*
 * \brief  Test for timeout library
 * \author Martin Stein
 * \date   2016-11-24
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <timer_session/connection.h>
#include <os/timer.h>


using namespace Genode;


class Main
{
	private:

		using Microseconds = Genode::Timer::Microseconds;

		void _handle(Microseconds now, Cstring name) {
			log(now.value / 1000, " ms: ", name, " timeout triggered"); }

		void _handle_pt1(Microseconds now) { _handle(now, "Periodic  700ms"); }
		void _handle_pt2(Microseconds now) { _handle(now, "Periodic 1000ms"); }
		void _handle_ot1(Microseconds now) { _handle(now, "One-shot 3250ms"); }
		void _handle_ot2(Microseconds now) { _handle(now, "One-shot 5200ms"); }

		Timer::Connection      _timer_connection;
		Genode::Timer          _timer;
		Periodic_timeout<Main> _pt1 { _timer, *this, &Main::_handle_pt1, Microseconds(700000) };
		Periodic_timeout<Main> _pt2 { _timer, *this, &Main::_handle_pt2, Microseconds(1000000)  };
		One_shot_timeout<Main> _ot1 { _timer, *this, &Main::_handle_ot1 };
		One_shot_timeout<Main> _ot2 { _timer, *this, &Main::_handle_ot2 };

	public:

		Main(Env &env) : _timer_connection(env),
		                 _timer(_timer_connection, env.ep())
		{
			_ot1.start(Microseconds(3250000));
			_ot2.start(Microseconds(5300000));
		}
};


void Component::construct(Env &env) { static Main main(env); }
