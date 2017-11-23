/*
 * \brief  Basic test for framebuffer session
 * \author Martin Stein
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2012-01-09
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>
#include <framebuffer_session/connection.h>
#include <base/attached_dataspace.h>
#include <util/reconstructible.h>


using Genode::uint16_t;

class Test_environment
{
	private:

		using Ds = Genode::Constructible<Genode::Attached_dataspace>;

		enum Color {
			BLACK = 0x0,
			BLUE  = 0x1f,
			GREEN = 0x7e0,
			RED   = 0xf800,
			WHITE = 0xffff,
		};

		enum State { STRIPES, ALL_BLUE, ALL_GREEN, ALL_RED, COLORED };

		Genode::Env &_env;

		Framebuffer::Mode                        _mode;
		Framebuffer::Connection                  _fb { _env, _mode };
		Ds                                       _fb_ds;
		Genode::Signal_handler<Test_environment> _mode_sigh;
		Genode::Signal_handler<Test_environment> _sync_sigh;
		unsigned long                            _sync_cnt = 0;
		State                                    _state = STRIPES;

		enum { FRAME_CNT = 200 };

		void _draw();
		void _mode_handle();

		void _sync_handle() {
			if (_sync_cnt++ % FRAME_CNT == 0) _draw(); }

		void _draw_frame(uint16_t volatile *, uint16_t, unsigned, unsigned);

		Genode::size_t _fb_bpp()  { return _mode.bytes_per_pixel(); }
		Genode::size_t _fb_size() { return _fb_ds->size(); }
		Genode::addr_t _fb_base() {
			return (Genode::addr_t) _fb_ds->local_addr<void>(); }

	public:

		Test_environment(Genode::Env &env)
		:
			_env(env),
			_mode_sigh(_env.ep(), *this, &Test_environment::_mode_handle),
			_sync_sigh(_env.ep(), *this, &Test_environment::_sync_handle)
		{
			_fb.mode_sigh(_mode_sigh);
			_fb.sync_sigh(_sync_sigh);
			_mode_handle();
		}
};


void Test_environment::_draw_frame(uint16_t volatile *p, uint16_t c,
                                   unsigned const w, unsigned const h)
{
	/* top and bottom */
	for (unsigned i = 0; i < w; ++i)
		p[i] = p[(h - 1)*w + i] = c;

	/* left and right */
	for (unsigned i = 0; i < h; ++i)
		p[i*w] = p[i*w + w - 1] = c;
}


void Test_environment::_draw()
{
	using namespace Genode;

	switch(_state) {
	case STRIPES:
		{
			Genode::log("black & white stripes");
			addr_t const stripe_width = _mode.width() / 4;
			addr_t stripe_o = 0;
			bool stripe = 0;
			for (addr_t o = 0; o < _fb_size(); o += _fb_bpp()) {
				stripe_o++;
				if (stripe_o == stripe_width) {
					stripe_o = 0;
					stripe = !stripe;
				}
				*(uint16_t volatile *)(_fb_base() + o) = stripe ? BLACK : WHITE;
			}

			_draw_frame((uint16_t volatile *)_fb_base(), RED,
			            _mode.width(), _mode.height());
			_state = ALL_BLUE;
			break;
		}
	case ALL_BLUE:
		{
			Genode::log("blue");
			for (addr_t o = 0; o < _fb_size(); o += _fb_bpp())
				*(uint16_t volatile *)(_fb_base() + o) = BLUE;

			_draw_frame((uint16_t volatile *)_fb_base(), RED,
			            _mode.width(), _mode.height());
			_state = ALL_GREEN;
			break;
		}
	case ALL_GREEN:
		{
			Genode::log("green");
			for (addr_t o = 0; o < _fb_size(); o += _fb_bpp())
				*(uint16_t volatile *)(_fb_base() + o) = GREEN;

			_draw_frame((uint16_t volatile *)_fb_base(), RED,
			            _mode.width(), _mode.height());
			_state = ALL_RED;
			break;
		}
	case ALL_RED:
		{
			Genode::log("red");
			for (addr_t o = 0; o < _fb_size(); o += _fb_bpp())
				*(uint16_t volatile *)(_fb_base() + o) = RED;

			_draw_frame((uint16_t volatile *)_fb_base(), WHITE,
			            _mode.width(), _mode.height());
			_state = COLORED;
			break;
		}
	case COLORED:
		{
			Genode::log("all colors mixed");
			unsigned i = 0;
			for (addr_t o = 0; o < _fb_size(); o += _fb_bpp(), i++)
				*(uint16_t volatile *)(_fb_base() + o) = i;

			_draw_frame((uint16_t volatile *)_fb_base(), WHITE,
			            _mode.width(), _mode.height());
			_state = STRIPES;
		}
	};
	_fb.refresh(0, 0, _mode.width(), _mode.height());
}


void Test_environment::_mode_handle()
{
	_mode = _fb.mode();
	if (_fb_ds.is_constructed())
		_fb_ds.destruct();

	_fb_ds.construct(_env.rm(), _fb.dataspace());

	Genode::log("framebuffer is ", _mode);

	if (_mode.bytes_per_pixel() != 2) {
		Genode::error("pixel format not supported");
		throw -1;
	}

	_draw();
}


void Component::construct(Genode::Env &env)
{
	Genode::log("--- Test framebuffer ---");
	static Test_environment te(env);
}
