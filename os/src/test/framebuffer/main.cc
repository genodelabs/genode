/*
 * \brief  Basic test for framebuffer session
 * \author Martin Stein
 * \date   2012-01-09
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
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

using namespace Genode;

int main()
{
	printf("--- Test framebuffer ---\n");
	static Timer::Connection timer;

	/* create framebuffer */
	static Framebuffer::Connection fb;
	Framebuffer::Mode const mode = fb.mode();
	PINF("framebuffer is %dx%d@%d\n", mode.width(), mode.height(), mode.format());
	Dataspace_capability fb_ds_cap = fb.dataspace();
	if (!fb_ds_cap.valid()) {
		PERR("Could not request dataspace for frame buffer");
		return -2;
	}
	/* drive framebuffer */
	addr_t const   fb_base = (addr_t)env()->rm_session()->attach(fb_ds_cap);
	unsigned const fb_bpp  = (unsigned)mode.bytes_per_pixel();
	if (fb_bpp != 2) {
		PERR("pixel format not supported");
		return -1;
	}
	unsigned const fb_size = (unsigned)(mode.width() * mode.height() * fb_bpp);
	while (1) {
		enum {
			BLACK = 0x0,
			BLUE  = 0x1f,
			GREEN = 0x7e0,
			RED   = 0xf800,
			WHITE = 0xffff,
		};
		for (addr_t o = 0; o < fb_size; o += fb_bpp)
			*(uint16_t volatile *)(fb_base + o) = BLACK;
		PINF("black");
		timer.msleep(2000);
		for (addr_t o = 0; o < fb_size; o += fb_bpp)
			*(uint16_t volatile *)(fb_base + o) = BLUE;
		PINF("blue");
		timer.msleep(2000);
		for (addr_t o = 0; o < fb_size; o += fb_bpp)
			*(uint16_t volatile *)(fb_base + o) = GREEN;
		PINF("green");
		timer.msleep(2000);
		for (addr_t o = 0; o < fb_size; o += fb_bpp)
			*(uint16_t volatile *)(fb_base + o) = RED;
		PINF("red");
		timer.msleep(2000);
		for (addr_t o = 0; o < fb_size; o += fb_bpp)
			*(uint16_t volatile *)(fb_base + o) = WHITE;
		PINF("white");
		timer.msleep(2000);
		unsigned i = 0;
		for (addr_t o = 0; o < fb_size; o += fb_bpp, i++)
			*(uint16_t volatile *)(fb_base + o) = i;
		PINF("all");
		timer.msleep(2000);
	}
	return 0;
}

