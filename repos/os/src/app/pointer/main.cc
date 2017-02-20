/*
 * \brief  Minimalistic nitpicker pointer
 * \author Norman Feske
 * \date   2014-07-02
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <nitpicker_session/connection.h>
#include <os/surface.h>
#include <os/pixel_rgb565.h>
#include <base/attached_dataspace.h>
#include <base/component.h>

/* local includes */
#include "big_mouse.h"


template <typename PT>
void convert_cursor_data_to_pixels(PT *pixel, Nitpicker::Area size)
{
	unsigned char *alpha = (unsigned char *)(pixel + size.count());

	for (unsigned y = 0; y < size.h(); y++) {
		for (unsigned x = 0; x < size.w(); x++) {

			/* the source is known to be in RGB565 format */
			Genode::Pixel_rgb565 src =
				*(Genode::Pixel_rgb565 *)(&big_mouse.pixels[y][x]);

			unsigned const i = y*size.w() + x;
			pixel[i] = PT(src.r(), src.g(), src.b());
			alpha[i] = src.r() ? 255 : 0;
		}
	}
}


void Component::construct(Genode::Env &env)
{
	static Nitpicker::Connection nitpicker { env, "cursor" };

	Nitpicker::Area const cursor_size(big_mouse.w, big_mouse.h);

	Framebuffer::Mode const mode(cursor_size.w(), cursor_size.h(),
	                             Framebuffer::Mode::RGB565);
	nitpicker.buffer(mode, true);

	Genode::Attached_dataspace ds(
		env.rm(), nitpicker.framebuffer()->dataspace());

	convert_cursor_data_to_pixels(
		ds.local_addr<Genode::Pixel_rgb565>(), cursor_size);

	Nitpicker::Session::View_handle view = nitpicker.create_view();
	Nitpicker::Rect geometry(Nitpicker::Point(0, 0), cursor_size);
	nitpicker.enqueue<Nitpicker::Session::Command::Geometry>(view, geometry);
	nitpicker.enqueue<Nitpicker::Session::Command::To_front>(view);
	nitpicker.execute();
}
