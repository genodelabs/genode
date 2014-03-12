/*
 * \brief  Basic test for framebuffer session
 * \author Martin Stein
 * \author Christian Helmuth
 * \date   2012-01-09
 */

/*
 * Copyright (C) 2012-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <framebuffer_session/connection.h>
#include <dataspace/client.h>
#include <base/printf.h>
#include <base/env.h>
#include <timer_session/connection.h>


class Test_environment
{
	private:

		Timer::Connection                  _timer;
		Framebuffer::Connection            _fb;
		Framebuffer::Mode            const _mode{_fb.mode()};

		Genode::Dataspace_capability const _ds_cap{_fb.dataspace()};
		void *                       const _base{Genode::env()->rm_session()->attach(_ds_cap)};

	public:

		Test_environment()
		{
			PINF("framebuffer is %dx%d@%d",
			     _mode.width(), _mode.height(), _mode.format());

			if (_mode.bytes_per_pixel() != 2) {
				PERR("pixel format not supported");
				throw -1;
			}
		}

		void complete_step()
		{
			_fb.refresh(0, 0, _mode.width(), _mode.height());
			_timer.msleep(2000);
		}

		Framebuffer::Mode fb_mode() const { return _mode; }
		unsigned fb_bpp()           const { return _mode.bytes_per_pixel(); }
		Genode::addr_t fb_base()    const { return (Genode::addr_t) _base; }
		Genode::size_t fb_size()    const { return _mode.width()
		                                         * _mode.height()
		                                         * _mode.bytes_per_pixel(); }
};


int main()
{
	using namespace Genode;

	printf("--- Test framebuffer ---\n");
	static Test_environment te;

	addr_t const stripe_width = te.fb_mode().width() / 4;
	while (1) {
		enum {
			BLACK = 0x0,
			BLUE  = 0x1f,
			GREEN = 0x7e0,
			RED   = 0xf800,
			WHITE = 0xffff,
		};
		PINF("black & white stripes");
		addr_t stripe_o = 0;
		bool stripe = 0;
		for (addr_t o = 0; o < te.fb_size(); o += te.fb_bpp()) {
			stripe_o++;
			if (stripe_o == stripe_width) {
				stripe_o = 0;
				stripe = !stripe;
			}
			*(uint16_t volatile *)(te.fb_base() + o) = stripe ? BLACK : WHITE;
		}
		te.complete_step();

		PINF("blue");
		for (addr_t o = 0; o < te.fb_size(); o += te.fb_bpp())
			*(uint16_t volatile *)(te.fb_base() + o) = BLUE;
		te.complete_step();

		PINF("green");
		for (addr_t o = 0; o < te.fb_size(); o += te.fb_bpp())
			*(uint16_t volatile *)(te.fb_base() + o) = GREEN;
		te.complete_step();

		PINF("red");
		for (addr_t o = 0; o < te.fb_size(); o += te.fb_bpp())
			*(uint16_t volatile *)(te.fb_base() + o) = RED;
		te.complete_step();

		PINF("all colors mixed");
		unsigned i = 0;
		for (addr_t o = 0; o < te.fb_size(); o += te.fb_bpp(), i++)
			*(uint16_t volatile *)(te.fb_base() + o) = i;
		te.complete_step();
	}
}

