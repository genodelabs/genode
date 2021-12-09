/*
 * \brief  Utility for the buffered pixel output via the GUI server interface
 * \author Norman Feske
 * \date   2014-08-22
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__GUI_BUFFER_H_
#define _INCLUDE__GEMS__GUI_BUFFER_H_

/* Genode includes */
#include <base/ram_allocator.h>
#include <gui_session/connection.h>
#include <base/attached_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <os/surface.h>
#include <os/pixel_alpha8.h>
#include <os/pixel_rgb888.h>
#include <blit/painter.h>


struct Gui_buffer
{
	typedef Genode::Pixel_rgb888 Pixel_rgb888;
	typedef Genode::Pixel_alpha8 Pixel_alpha8;

	typedef Genode::Surface<Pixel_rgb888> Pixel_surface;
	typedef Genode::Surface<Pixel_alpha8> Alpha_surface;

	typedef Genode::Surface_base::Area  Area;
	typedef Genode::Surface_base::Rect  Rect;
	typedef Genode::Surface_base::Point Point;

	typedef Genode::Attached_ram_dataspace Ram_ds;

	using size_t = Genode::size_t;

	Genode::Ram_allocator &ram;
	Genode::Region_map    &rm;

	Gui::Connection &gui;

	Framebuffer::Mode const mode;

	/**
	 * Return dataspace capability for virtual framebuffer
	 */
	Genode::Dataspace_capability _ds_cap(Gui::Connection &gui)
	{
		/* setup virtual framebuffer mode */
		gui.buffer(mode, true);

		return gui.framebuffer()->dataspace();
	}

	Genode::Attached_dataspace fb_ds { rm, _ds_cap(gui) };

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
	Gui_buffer(Gui::Connection &gui, Area size,
	           Genode::Ram_allocator &ram, Genode::Region_map &rm)
	:
		ram(ram), rm(rm), gui(gui),
		mode({ .area = { Genode::max(1U, size.w()),
		                 Genode::max(1U, size.h()) } })
	{
		reset_surface();
	}

	/**
	 * Return size of virtual framebuffer
	 */
	Area size() const { return mode.area; }

	template <typename FN>
	void apply_to_surface(FN const &fn)
	{
		Pixel_surface pixel(pixel_surface_ds.local_addr<Pixel_rgb888>(), size());
		Alpha_surface alpha(alpha_surface_ds.local_addr<Pixel_alpha8>(), size());
		fn(pixel, alpha);
	}

	void reset_surface()
	{
		Pixel_surface pixel(pixel_surface_ds.local_addr<Pixel_rgb888>(), size());
		Alpha_surface alpha(alpha_surface_ds.local_addr<Pixel_alpha8>(), size());

		Genode::size_t const num_pixels = size().count();
		Genode::memset(alpha.addr(), 0, num_pixels);

		/*
		 * Initialize color buffer with 50% gray
		 *
		 * We do not use black to limit the bleeding of black into antialiased
		 * drawing operations applied onto an initially transparent background.
		 */
		Pixel_rgb888 *dst = pixel.addr();

		Pixel_rgb888 const gray(127, 127, 127, 255);

		for (size_t n = num_pixels; n; n--)
			*dst++ = gray;
	}

	template <typename DST_PT, typename SRC_PT>
	void _convert_back_to_front(DST_PT                        *front_base,
	                            Genode::Texture<SRC_PT> const &texture,
	                            Rect                    const  clip_rect)
	{
		Genode::Surface<DST_PT> surface(front_base, size());

		surface.clip(clip_rect);

		Blit_painter::paint(surface, texture, Point());
	}

	void _update_input_mask()
	{
		size_t const num_pixels = size().count();

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
			pixel_texture(pixel_surface_ds.local_addr<Pixel_rgb888>(),
			              nullptr, size());

		Genode::Texture<Pixel_alpha8>
			alpha_texture(alpha_surface_ds.local_addr<Pixel_alpha8>(),
			              nullptr, size());

		// XXX track dirty rectangles
		Rect const clip_rect(Genode::Surface_base::Point(0, 0), size());

		Pixel_rgb888 *pixel_base = fb_ds.local_addr<Pixel_rgb888>();
		Pixel_alpha8 *alpha_base = fb_ds.local_addr<Pixel_alpha8>()
		                         + mode.bytes_per_pixel()*size().count();

		_convert_back_to_front(pixel_base, pixel_texture, clip_rect);
		_convert_back_to_front(alpha_base, alpha_texture, clip_rect);

		_update_input_mask();
	}
};

#endif /* _INCLUDE__GEMS__GUI_BUFFER_H_ */
