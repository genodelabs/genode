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


struct Gui_buffer : Genode::Noncopyable
{
	using Pixel_rgb888  = Genode::Pixel_rgb888;
	using Pixel_alpha8  = Genode::Pixel_alpha8;
	using Pixel_surface = Genode::Surface<Pixel_rgb888>;
	using Alpha_surface = Genode::Surface<Pixel_alpha8>;
	using Area          = Genode::Surface_base::Area;
	using Rect          = Genode::Surface_base::Rect;
	using Point         = Genode::Surface_base::Point;
	using Ram_ds        = Genode::Attached_ram_dataspace;
	using size_t        = Genode::size_t;
	using uint8_t       = Genode::uint8_t;

	Genode::Ram_allocator &ram;
	Genode::Region_map    &rm;

	Gui::Connection &gui;

	Framebuffer::Mode const mode;

	Pixel_rgb888 const reset_color;

	/**
	 * Return dataspace capability for virtual framebuffer
	 */
	Genode::Dataspace_capability _ds_cap(Gui::Connection &gui)
	{
		/*
		 * Setup virtual framebuffer, the upper part containing the front
		 * buffer, the lower part containing the back buffer.
		 */
		gui.buffer({ .area  = { mode.area.w, mode.area.h*2 },
		             .alpha = mode.alpha });

		return gui.framebuffer.dataspace();
	}

	Genode::Attached_dataspace _fb_ds { rm, _ds_cap(gui) };

	size_t _pixel_num_bytes() const { return size().count()*sizeof(Pixel_rgb888); }
	size_t _alpha_num_bytes() const { return mode.alpha ? size().count() : 0; }
	size_t _input_num_bytes() const { return mode.alpha ? size().count() : 0; }

	void _with_pixel_ptr(auto const &fn)
	{
		/* skip pixel front buffer */
		uint8_t * const ptr = _fb_ds.local_addr<uint8_t>() + _pixel_num_bytes();
		fn((Pixel_rgb888 *)ptr);
	}

	void _with_alpha_ptr(auto const &fn)
	{
		if (!mode.alpha)
			return;

		/* skip pixel front buffer, pixel back buffer, and alpha front buffer */
		uint8_t * const ptr = _fb_ds.local_addr<uint8_t>()
		                    + _pixel_num_bytes()*2 + _alpha_num_bytes();
		fn((Pixel_alpha8 *)ptr);
	}

	void _with_input_ptr(auto const &fn)
	{
		if (!mode.alpha)
			return;

		/* skip pixel buffers, alpha buffers, and input front buffer */
		uint8_t * const ptr = _fb_ds.local_addr<uint8_t>()
		                    + _pixel_num_bytes()*2 + _alpha_num_bytes()*2 + _input_num_bytes();
		fn(ptr);
	}

	enum class Alpha { OPAQUE, ALPHA };

	static Genode::Color default_reset_color()
	{
		/*
		 * Do not use black by default to limit the bleeding of black into
		 * antialiased drawing operations applied onto an initially transparent
		 * background.
		 */
		return Genode::Color(127, 127, 127, 255);
	}

	/**
	 * Constructor
	 */
	Gui_buffer(Gui::Connection &gui, Area size,
	           Genode::Ram_allocator &ram, Genode::Region_map &rm,
	           Alpha alpha = Alpha::ALPHA,
	           Genode::Color reset_color = default_reset_color())
	:
		ram(ram), rm(rm), gui(gui),
		mode({ .area = { Genode::max(1U, size.w),
		                 Genode::max(1U, size.h) },
		       .alpha = (alpha == Alpha::ALPHA) }),
		reset_color(reset_color.r, reset_color.g, reset_color.b, reset_color.a)
	{
		reset_surface();
	}

	/**
	 * Return size of the drawing surface
	 */
	Area size() const { return mode.area; }

	void with_alpha_surface(auto const &fn)
	{
		_with_alpha_ptr([&] (Pixel_alpha8 *ptr) {
			Alpha_surface alpha { ptr, size() };
			fn(alpha); });
	}

	void with_pixel_surface(auto const &fn)
	{
		_with_pixel_ptr([&] (Pixel_rgb888 *ptr) {
			Pixel_surface pixel { ptr, size() };
			fn(pixel); });
	}

	void apply_to_surface(auto const &fn)
	{
		with_alpha_surface([&] (Alpha_surface &alpha) {
			with_pixel_surface([&] (Pixel_surface &pixel) {
				fn(pixel, alpha); }); });
	}

	void reset_surface()
	{
		with_alpha_surface([&] (Alpha_surface &alpha) {
			Genode::memset(alpha.addr(), 0, _alpha_num_bytes()); });

		with_pixel_surface([&] (Pixel_surface &pixel) {

			Pixel_rgb888 *dst = pixel.addr();
			Pixel_rgb888 const color = reset_color;

			for (size_t n = size().count(); n; n--)
				*dst++ = color;
		});
	}

	void _update_input_mask()
	{
		_with_alpha_ptr([&] (Pixel_alpha8 const * const alpha_ptr) {
			_with_input_ptr([&] (uint8_t *dst) {

				/*
				 * Set input mask for all pixels where the alpha value is above
				 * a given threshold. The threshold is defined such that
				 * typical drop shadows are below the value.
				 */
				uint8_t const   threshold  = 100;
				uint8_t const * src        = (uint8_t const *)alpha_ptr;
				size_t  const   num_pixels = size().count();

				for (unsigned i = 0; i < num_pixels; i++)
					*dst++ = (*src++) > threshold;
			});
		});
	}

	void flush_surface()
	{
		_update_input_mask();

		/* copy lower part of virtual framebuffer to upper part */
		gui.framebuffer.blit({ { 0, int(size().h) }, size() }, { 0, 0 });
	}
};

#endif /* _INCLUDE__GEMS__GUI_BUFFER_H_ */
