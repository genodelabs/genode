/*
 * \brief  Stress test for decorator
 * \author Norman Feske
 * \date   2014-04-28
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
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
				xml.attribute("xpos",   w * (0.25 + sin(param.angle[0])/5));
				xml.attribute("ypos",   h * (0.25 + sin(param.angle[1])/5));
				xml.attribute("width",  w * (0.25 + sin(param.angle[2])/5));
				xml.attribute("height", h * (0.25 + sin(param.angle[3])/5));

				if (i == 2)
					xml.attribute("focused", "yes");
			});

			param = param + Param(2.2, 3.3, 4.4, 5.5);
		}
	});
}


int main(int argc, char **argv)
{
	Param param(0, 1, 2, 3);

	Genode::Reporter window_layout_reporter("window_layout", 10*4096);
	window_layout_reporter.enabled(true);

	static Timer::Connection timer;

	for (;;) {

		report_window_layout(param, window_layout_reporter);

		param = param + Param(0.0331/2, 0.042/2, 0.051/2, 0.04/2);

		timer.msleep(10);
	}
	return 0;
}
