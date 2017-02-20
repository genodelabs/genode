/*
 * \brief  Stress test for decorator
 * \author Norman Feske
 * \date   2014-04-28
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <libc/component.h>
#include <util/list.h>
#include <util/geometry.h>
#include <timer_session/connection.h>
#include <os/reporter.h>
#include <os/surface.h>

/* libc includes */
#include <math.h>

typedef Genode::Surface_base::Point Point;
typedef Genode::Surface_base::Area  Area;
typedef Genode::Surface_base::Rect  Rect;


struct Param
{
	float angle[4];

	Param(float alpha, float beta, float gamma, float delta)
	: angle { alpha, beta, gamma, delta }
	{ }

	Param operator + (Param const &other)
	{
		return Param(fmod(angle[0] + other.angle[0], M_PI*2),
		             fmod(angle[1] + other.angle[1], M_PI*2),
		             fmod(angle[2] + other.angle[2], M_PI*2),
		             fmod(angle[3] + other.angle[3], M_PI*2));
	}
};


void report_window_layout(Param param, Genode::Reporter &reporter)
{

	float w = 1024;
	float h = 768;

	Genode::Reporter::Xml_generator xml(reporter, [&] ()
	{
		for (unsigned i = 1; i <= 10; i++) {

			xml.node("window", [&] ()
			{
				xml.attribute("id", i);
				xml.attribute("xpos",   (long)(w * (0.25 + sin(param.angle[0])/5)));
				xml.attribute("ypos",   (long)(h * (0.25 + sin(param.angle[1])/5)));
				xml.attribute("width",  (long)(w * (0.25 + sin(param.angle[2])/5)));
				xml.attribute("height", (long)(h * (0.25 + sin(param.angle[3])/5)));

				if (i == 2)
					xml.attribute("focused", "yes");
			});

			param = param + Param(2.2, 3.3, 4.4, 5.5);
		}
	});
}


struct Main
{
	Genode::Env &_env;

	Param _param { 0, 1, 2, 3 };

	Genode::Reporter _window_layout_reporter { _env, "window_layout", "window_layout", 10*4096 };

	Timer::Connection _timer { _env };

	void _handle_timer()
	{
		report_window_layout(_param, _window_layout_reporter);

		_param = _param + Param(0.0331/2, 0.042/2, 0.051/2, 0.04/2);
	}

	Genode::Signal_handler<Main> _timer_handler {
		_env.ep(), *this, &Main::_handle_timer };

	Main(Genode::Env &env) : _env(env)
	{
		_window_layout_reporter.enabled(true);
		_timer.sigh(_timer_handler);
		_timer.trigger_periodic(10*1000);
	}
};


void Libc::Component::construct(Libc::Env &env) { static Main main(env); }

