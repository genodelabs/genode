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
#include <gui_session/connection.h>
#include <base/attached_ram_dataspace.h>


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

	Genode::Ram_allocator &_ram;
	Genode::Region_map    &_rm;

	Gui::Connection &_gui;

	Framebuffer::Mode const mode;

	/*
	 * Make the GUI mode twice as high as the requested mode. The upper part
	 * of the GUI framebuffer contains the front buffer, the lower part
	 * contains the back buffer.
	 */
	Framebuffer::Mode const _gui_mode { .area  = { mode.area.w, mode.area.h*2 },
	                                    .alpha = mode.alpha };

	Genode::Surface_window const _backbuffer { .y = mode.area.h, .h = mode.area.h };

	Pixel_rgb888 const reset_color;

	Genode::Attached_dataspace _fb_ds {
		_rm, ( _gui.buffer(_gui_mode), _gui.framebuffer.dataspace() ) };

	enum class Alpha { OPAQUE, ALPHA };

	/*
	 * Do not use black by default to limit the bleeding of black into
	 * antialiased drawing operations applied onto an initially transparent
	 * background.
	 */
	static constexpr Genode::Color default_reset_color = { 127, 127, 127, 255 };

	/**
	 * Constructor
	 */
	Gui_buffer(Gui::Connection &gui, Area size,
	           Genode::Ram_allocator &ram, Genode::Region_map &rm,
	           Alpha alpha = Alpha::ALPHA,
	           Genode::Color reset_color = default_reset_color)
	:
		_ram(ram), _rm(rm), _gui(gui),
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
		if (!_gui_mode.alpha) {
			Alpha_surface dummy { nullptr, Gui::Area { } };
			fn(dummy);
			return;
		}
		_gui_mode.with_alpha_surface(_fb_ds, [&] (Alpha_surface &surface) {
			surface.with_window(_backbuffer, [&] (Alpha_surface &surface) {
				fn(surface); }); });
	}

	void with_pixel_surface(auto const &fn)
	{
		_gui_mode.with_pixel_surface(_fb_ds, [&] (Pixel_surface &surface) {
			surface.with_window(_backbuffer, [&] (Pixel_surface &surface) {
				fn(surface); }); });
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
			Genode::memset(alpha.addr(), 0, alpha.size().count()); });

		with_pixel_surface([&] (Pixel_surface &pixel) {

			Pixel_rgb888 *dst = pixel.addr();
			Pixel_rgb888 const color = reset_color;

			for (size_t n = size().count(); n; n--)
				*dst++ = color;
		});
	}

	void _update_input_mask()
	{
		with_alpha_surface([&] (Alpha_surface &alpha) {

			using Input_surface = Genode::Surface<Genode::Pixel_input8>;

			_gui_mode.with_input_surface(_fb_ds, [&] (Input_surface &input) {
				input.with_window(_backbuffer, [&] (Input_surface &input) {

					uint8_t const * src = (uint8_t *)alpha.addr();
					uint8_t       * dst = (uint8_t *)input.addr();

					/*
					 * Set input mask for all pixels where the alpha value is
					 * above a given threshold. The threshold is defined such
					 * that typical drop shadows are below the value.
					 */
					uint8_t const threshold  = 100;
					size_t  const num_pixels = Genode::min(alpha.size().count(),
					                                       input.size().count());

					for (unsigned i = 0; i < num_pixels; i++)
						*dst++ = (*src++) > threshold;
				});
			});
		});
	}

	void flush_surface()
	{
		_update_input_mask();

		/* copy lower part of virtual framebuffer to upper part */
		_gui.framebuffer.blit({ { 0, int(size().h) }, size() }, { 0, 0 });
	}
};

#endif /* _INCLUDE__GEMS__GUI_BUFFER_H_ */
