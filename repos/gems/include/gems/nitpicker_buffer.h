/*
 * \brief  Utility for the buffered pixel output via nitpicker
 * \author Norman Feske
 * \date   2014-08-22
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__NITPICKER_BUFFER_H_
#define _INCLUDE__GEMS__NITPICKER_BUFFER_H_

/* Genode includes */
#include <ram_session/ram_session.h>
#include <nitpicker_session/connection.h>
#include <base/attached_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <os/surface.h>
#include <os/pixel_rgb565.h>
#include <os/pixel_alpha8.h>
#include <os/pixel_rgb888.h>

/* gems includes */
#include <gems/dither_painter.h>


struct Nitpicker_buffer
{
	typedef Genode::Pixel_rgb888 Pixel_rgb888;
	typedef Genode::Pixel_rgb565 Pixel_rgb565;
	typedef Genode::Pixel_alpha8 Pixel_alpha8;

	typedef Genode::Surface<Pixel_rgb888> Pixel_surface;
	typedef Genode::Surface<Pixel_alpha8> Alpha_surface;

	typedef Genode::Surface_base::Area  Area;
	typedef Genode::Surface_base::Rect  Rect;
	typedef Genode::Surface_base::Point Point;

	typedef Genode::Attached_ram_dataspace Ram_ds;

	Genode::Ram_session &ram;
	Genode::Region_map  &rm;

	Nitpicker::Connection &nitpicker;

	Framebuffer::Mode const mode;

	/**
	 * Return dataspace capability for virtual framebuffer
	 */
	Genode::Dataspace_capability _ds_cap(Nitpicker::Connection &nitpicker)
	{
		/* setup virtual framebuffer mode */
		nitpicker.buffer(mode, true);

		if (mode.format() != Framebuffer::Mode::RGB565) {
			Genode::warning("color mode ", mode, " not supported");
			return Genode::Dataspace_capability();
		}

		return nitpicker.framebuffer()->dataspace();
	}

	Genode::Attached_dataspace fb_ds { rm, _ds_cap(nitpicker) };

	Genode::size_t pixel_surface_num_bytes() const
	{
		return size().count()*sizeof(Pixel_rgb888);
	}

	Genode::size_t alpha_surface_num_bytes() const
	{
		return size().count();
	}

	Ram_ds pixel_surface_ds { ram, rm, pixel_surface_num_bytes() };
	Ram_ds alpha_surface_ds { ram, rm, alpha_surface_num_bytes() };

	/**
	 * Constructor
	 */
	Nitpicker_buffer(Nitpicker::Connection &nitpicker, Area size,
	                 Genode::Ram_session &ram, Genode::Region_map &rm)
	:
		ram(ram), rm(rm), nitpicker(nitpicker),
		mode(Genode::max(1UL, size.w()), Genode::max(1UL, size.h()),
		     nitpicker.mode().format())
	{
		reset_surface();
	}

	/**
	 * Constructor
	 *
	 * \deprecated
	 * \noapi
	 */
	Nitpicker_buffer(Nitpicker::Connection &nitpicker, Area size,
	                 Genode::Ram_session &ram) __attribute__((deprecated))
	:
		ram(ram), rm(*Genode::env_deprecated()->rm_session()), nitpicker(nitpicker),
		mode(Genode::max(1UL, size.w()), Genode::max(1UL, size.h()),
		     nitpicker.mode().format())
	{ }

	/**
	 * Return size of virtual framebuffer
	 */
	Area size() const
	{
		return Area(mode.width(), mode.height());
	}

	/**
	 * Return back buffer as RGB888 painting surface
	 */
	Pixel_surface pixel_surface()
	{
		return Pixel_surface(pixel_surface_ds.local_addr<Pixel_rgb888>(), size());
	}

	Alpha_surface alpha_surface()
	{
		return Alpha_surface(alpha_surface_ds.local_addr<Pixel_alpha8>(), size());
	}

	void reset_surface()
	{
		Genode::size_t const num_pixels = pixel_surface().size().count();
		Genode::memset(alpha_surface().addr(), 0, num_pixels);

		/*
		 * Initialize color buffer with 50% gray
		 *
		 * We do not use black to limit the bleeding of black into antialiased
		 * drawing operations applied onto an initially transparent background.
		 */
		Pixel_surface pixels = pixel_surface();
		Pixel_rgb888 *dst    = pixels.addr();

		Pixel_rgb888 const gray(127, 127, 127, 255);

		for (unsigned n = pixels.size().count(); n; n--)
			*dst++ = gray;
	}

	template <typename DST_PT, typename SRC_PT>
	void _convert_back_to_front(DST_PT                        *front_base,
	                            Genode::Texture<SRC_PT> const &texture,
	                            Rect                    const  clip_rect)
	{
		Genode::Surface<DST_PT> surface(front_base, size());

		surface.clip(clip_rect);

		Dither_painter::paint(surface, texture, Point());
	}

	void _update_input_mask()
	{
		unsigned const num_pixels = size().count();

		unsigned char * const alpha_base = fb_ds.local_addr<unsigned char>()
		                                 + mode.bytes_per_pixel()*num_pixels;

		unsigned char * const input_base = alpha_base + num_pixels;

		unsigned char const *src = alpha_base;
		unsigned char       *dst = input_base;

		/*
		 * Set input mask for all pixels where the alpha value is above a
		 * given threshold. The threshold is defines such that typical
		 * drop shadows are below the value.
		 */
		unsigned char const threshold = 100;

		for (unsigned i = 0; i < num_pixels; i++)
			*dst++ = (*src++) > threshold;
	}

	void flush_surface()
	{
		/* represent back buffer as texture */
		Genode::Texture<Pixel_rgb888>
			texture(pixel_surface_ds.local_addr<Pixel_rgb888>(),
			        alpha_surface_ds.local_addr<unsigned char>(),
			        size());

		// XXX track dirty rectangles
		Rect const clip_rect(Genode::Surface_base::Point(0, 0), size());

		Pixel_rgb565 *pixel_base = fb_ds.local_addr<Pixel_rgb565>();
		Pixel_alpha8 *alpha_base = fb_ds.local_addr<Pixel_alpha8>()
		                         + mode.bytes_per_pixel()*size().count();

		_convert_back_to_front(pixel_base, texture, clip_rect);
		_convert_back_to_front(alpha_base, texture, clip_rect);

		_update_input_mask();
	}
};

#endif /* _INCLUDE__GEMS__NITPICKER_BUFFER_H_ */
