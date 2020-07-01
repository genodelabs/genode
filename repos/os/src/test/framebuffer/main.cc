/*
 * \brief  Basic test for framebuffer drivers
 * \author Martin Stein
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \author Norman Feske
 * \date   2012-01-09
 */

/*
 * Copyright (C) 2012-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>
#include <os/static_root.h>
#include <capture_session/capture_session.h>
#include <base/attached_ram_dataspace.h>
#include <util/reconstructible.h>

namespace Test {

	using namespace Genode;

	using Area  = Capture::Area;
	using Point = Capture::Point;
	using Pixel = Capture::Pixel;

	struct Capture_session;
	class Main;
}


struct Test::Capture_session : Rpc_object<Capture::Session>
{
	Env &_env;

	Pixel const BLACK = {   0,   0,   0 };
	Pixel const BLUE  = {   0,   0, 255 };
	Pixel const GREEN = {   0, 255,   0 };
	Pixel const RED   = { 255,   0,   0 };
	Pixel const WHITE = { 255, 255, 255 };

	enum State { STRIPES, ALL_BLUE, ALL_GREEN, ALL_RED, COLORED };

	Area _size { 0, 0 };

	Constructible<Attached_ram_dataspace> _ds { };

	State _state = STRIPES;

	unsigned long _sync_cnt = 0;

	enum { FRAME_CNT = 200 };

	void _draw();

	void _draw_frame(Pixel *, Pixel, Area);

	Capture_session(Env &env) : _env(env) { }


	/********************************
	 ** Capture::Session interface **
	 ********************************/

	Area screen_size() const override { return _size; }

	void screen_size_sigh(Signal_context_capability) override { }

	void buffer(Area size) override
	{
		_ds.construct(_env.ram(), _env.rm(), buffer_bytes(size));
		_size = size;
		_draw();
	}

	Dataspace_capability dataspace() override
	{
		return _ds.constructed() ? _ds->cap() : Ram_dataspace_capability();
	}

	Affected_rects capture_at(Point) override
	{
		Affected_rects affected { };

		if (_sync_cnt++ % FRAME_CNT == 0) {
			_draw();
			affected.rects[0] = Rect(Point(0, 0), _size);
		}

		return affected;
	}
};


class Test::Main
{
	private:

		Env &_env;

		Capture_session _capture_session { _env };

		Static_root<Capture::Session> _capture_root { _env.ep().manage(_capture_session) };

	public:

		Main(Env &env) : _env(env)
		{
			_env.parent().announce(env.ep().manage(_capture_root));
		}
};


void Test::Capture_session::_draw_frame(Pixel *p, Pixel c, Area area)
{
	unsigned const w = area.w(), h = area.h();

	/* top and bottom */
	for (unsigned i = 0; i < w; ++i)
		p[i] = p[(h - 1)*w + i] = c;

	/* left and right */
	for (unsigned i = 0; i < h; ++i)
		p[i*w] = p[i*w + w - 1] = c;
}


void Test::Capture_session::_draw()
{
	if (!_ds.constructed())
		return;

	addr_t const num_pixels = _size.count();

	Pixel * const fb_base = _ds->local_addr<Pixel>();

	switch(_state) {
	case STRIPES:
		{
			log("black & white stripes");
			addr_t const stripe_width = _size.w() / 4;
			addr_t stripe_o = 0;
			bool stripe = 0;
			for (addr_t o = 0; o < num_pixels; o++) {
				stripe_o++;
				if (stripe_o == stripe_width) {
					stripe_o = 0;
					stripe = !stripe;
				}
				fb_base[o] = stripe ? BLACK : WHITE;
			}

			_draw_frame(fb_base, RED, _size);
			_state = ALL_BLUE;
			break;
		}
	case ALL_BLUE:
		{
			log("blue");
			for (addr_t o = 0; o < num_pixels; o++)
				fb_base[o] = BLUE;

			_draw_frame(fb_base, RED, _size);
			_state = ALL_GREEN;
			break;
		}
	case ALL_GREEN:
		{
			log("green");
			for (addr_t o = 0; o < num_pixels; o++)
				fb_base[o] = GREEN;

			_draw_frame(fb_base, RED, _size);
			_state = ALL_RED;
			break;
		}
	case ALL_RED:
		{
			log("red");
			for (addr_t o = 0; o < num_pixels; o++)
				fb_base[o] = RED;

			_draw_frame(fb_base, WHITE, _size);
			_state = COLORED;
			break;
		}
	case COLORED:
		{
			log("all colors mixed");
			unsigned i = 0;
			for (addr_t o = 0; o < num_pixels; o++, i++)
				fb_base[o] = Pixel(i>>16, i>>8, i);

			_draw_frame(fb_base, WHITE, _size);
			_state = STRIPES;
		}
	};
}


void Component::construct(Genode::Env &env)
{
	Genode::log("--- Test framebuffer ---");
	static Test::Main main(env);
}
