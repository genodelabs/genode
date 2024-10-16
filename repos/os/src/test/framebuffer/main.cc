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
#include <timer_session/connection.h>
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

	void _draw();
	void _draw_frame(Pixel *, Pixel, Area);

	bool _dirty           = false; /* true if there is data not yet delivered */
	bool _capture_stopped = false;

	Signal_context_capability _wakeup_sigh { };

	void _wakeup_if_needed()
	{
		if (_capture_stopped && _dirty && _wakeup_sigh.valid()) {
			Signal_transmitter(_wakeup_sigh).submit();
			_capture_stopped = false;
		}
	}

	Timer::Connection _timer { _env };

	Signal_handler<Capture_session> _timer_handler {
		_env.ep(), *this, &Capture_session::_handle_timer };

	void _handle_timer()
	{
		_dirty = true;
		_wakeup_if_needed();
	}

	Capture_session(Env &env) : _env(env)
	{
		_timer.sigh(_timer_handler);
		_timer.trigger_periodic(1000*1000);
	}


	/********************************
	 ** Capture::Session interface **
	 ********************************/

	Area screen_size() const override { return _size; }

	void screen_size_sigh(Signal_context_capability) override { }

	void wakeup_sigh(Signal_context_capability sigh) override { _wakeup_sigh = sigh; }

	Buffer_result buffer(Buffer_attr attr) override
	{
		try {
			_ds.construct(_env.ram(), _env.rm(), buffer_bytes(attr.px));
		}
		catch (Out_of_ram)  { return Buffer_result::OUT_OF_RAM;  }
		catch (Out_of_caps) { return Buffer_result::OUT_OF_CAPS; }

		_size = attr.px;

		log("screen dimension: ", _size);

		_draw();
		return Buffer_result::OK;
	}

	Dataspace_capability dataspace() override
	{
		return _ds.constructed() ? _ds->cap() : Ram_dataspace_capability();
	}

	Affected_rects capture_at(Point) override
	{
		Affected_rects affected { };
		if (_dirty) {
			_draw();
			affected.rects[0] = Rect { { }, _size };
			_dirty = false;
		}
		return affected;
	}

	void capture_stopped() override { _capture_stopped = true; }
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
	unsigned const w = area.w, h = area.h;

	/* top and bottom */
	for (unsigned i = 0; i < w; ++i)
		p[i] = p[(h - 1)*w + i] = c;

	/* left and right */
	for (unsigned i = 0; i < h; ++i)
		p[i*w] = p[i*w + w - 1] = c;

	/* 15px marker for (0,0) */
	for (unsigned i = 0; i < 15; i++)
		for (unsigned j = 0; j < 15; j++)
			p[i*w + j] =  c;
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
			addr_t const stripe_width = _size.w / 4;
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
