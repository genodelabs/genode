/*
 * \brief  Loader test program
 * \author Christian Prochaska
 * \date   2011-07-07
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <loader_session/connection.h>
#include <timer_session/connection.h>

namespace Test {
	using namespace Genode;
	struct Main;
}


struct Test::Main
{
	Env &_env;

	Loader::Connection _loader { _env, 8*1024*1024 };
	Timer::Connection  _timer  { _env };

	Loader::Area  _size;
	Loader::Point _pos;

	void _handle_view_ready()
	{
		_size = _loader.view_size();
		_timer.trigger_periodic(250*1000);
	}

	Signal_handler<Main> _view_ready_handler {
		_env.ep(), *this, &Main::_handle_view_ready };

	void _handle_timer()
	{
		_loader.view_geometry(Loader::Rect(_pos, _size), Loader::Point(0, 0));
		_pos = Loader::Point((_pos.x() + 50) % 500, (_pos.y() + 30) % 300);
	}

	Signal_handler<Main> _timer_handler {
		_env.ep(), *this, &Main::_handle_timer };

	Main(Env &env) : _env(env)
	{
		_loader.view_ready_sigh(_view_ready_handler);
		_timer.sigh(_timer_handler);

		_loader.start("testnit", "test-label");
	}
};


void Component::construct(Genode::Env &env) { static Test::Main main(env); }
