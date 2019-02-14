/*
 * \brief   Generic interface of graphics backend and chunky template
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT__CANVAS_H_
#define _INCLUDE__SCOUT__CANVAS_H_

#include <base/allocator.h>
#include <scout/texture_allocator.h>
#include <os/pixel_rgba.h>
#include <util/color.h>
#include <util/dither_matrix.h>

#include <nitpicker_gfx/texture_painter.h>
#include <nitpicker_gfx/text_painter.h>
#include <nitpicker_gfx/box_painter.h>
#include <scout_gfx/icon_painter.h>
#include <scout_gfx/sky_texture_painter.h>
#include <scout_gfx/horizontal_shadow_painter.h>
#include <scout_gfx/refracted_icon_painter.h>

namespace Scout {
	using Genode::Color;
	using Genode::Pixel_rgba;

	typedef Text_painter::Font Font;

	struct Canvas_base;
	template <typename PT> class Canvas;
}


struct Scout::Canvas_base : Texture_allocator
{
	virtual ~Canvas_base() { }

	virtual Area size() const = 0;

	virtual Rect clip() const = 0;

	virtual void clip(Rect rect) = 0;

	virtual void draw_box(int x, int y, int w, int h, Color c) = 0;

	virtual void draw_string(int x, int y, Font *font, Color color,
	                         char const *str, int len) = 0;

	virtual void draw_horizontal_shadow(Rect rect, int intensity) = 0;

	virtual void draw_icon(Rect, Texture_base const &, unsigned alpha) = 0;

	virtual void draw_sky_texture(int py,
	                              Sky_texture_painter::Sky_texture_base const &,
	                              bool detail) = 0;

	virtual void draw_refracted_icon(Point pos,
	                                 Scout::Refracted_icon_painter::Distmap<short> const &distmap,
	                                 Texture_base &tmp, Texture_base const &foreground,
	                                 bool detail, bool filter_backbuf) = 0;

	virtual void draw_texture(Point pos, Texture_base const &texture) = 0;
};


#include <os/texture_rgb565.h>
#include <base/env.h>


template <typename PT>
class Scout::Canvas : public Canvas_base
{
	private:

		Genode::Allocator  &_alloc;
		Genode::Surface<PT> _surface;

	public:

		/**
		 * Constructor
		 *
		 * \param alloc  allocator to be used for allocating textures
		 */
		Canvas(PT *base, Area size, Genode::Allocator &alloc)
		: _alloc(alloc), _surface(base, size) { }

		Area size() const override { return _surface.size(); }

		Rect clip() const override { return _surface.clip(); }

		void clip(Rect rect) override { _surface.clip(rect); }

		void draw_string(int x, int y, Font *font, Color color, char const *str, int len) override
		{
			char buf[len + 1];
			Genode::strncpy(buf, str, len + 1);
			Text_painter::paint(_surface, Text_painter::Position(x, y), *font, color, buf);
		}

		void draw_box(int x, int y, int w, int h, Color c) override
		{
			Box_painter::paint(_surface, Rect(Point(x, y), Area(w, h)), c);
		}

		void draw_horizontal_shadow(Rect rect, int intensity) override
		{
			Horizontal_shadow_painter::paint(_surface, rect, intensity);
		}

		void draw_icon(Rect rect, Texture_base const &icon, unsigned alpha) override
		{
			Icon_painter::paint(_surface, rect,
			                    static_cast<Texture<PT> const &>(icon), alpha);
		}

		void draw_sky_texture(int py,
		                      Sky_texture_painter::Sky_texture_base const &texture,
		                      bool detail) override
		{
			Sky_texture_painter::paint(_surface, py, texture, detail);
		}

		void draw_refracted_icon(Point pos,
		                         Scout::Refracted_icon_painter::Distmap<short> const &distmap,
		                         Texture_base &tmp, Texture_base const &foreground,
		                         bool detail, bool filter_backbuf) override
		{
			using namespace Scout;
			Refracted_icon_painter::paint(_surface, pos, distmap,
			                              static_cast<Texture<PT> &>(tmp),
			                              static_cast<Texture<PT> const &>(foreground),
			                              detail, filter_backbuf);
		}

		void draw_texture(Point pos, Texture_base const &texture_base) override
		{
			Texture<PT> const &texture = static_cast<Texture<PT> const &>(texture_base);

			Texture_painter::paint(_surface, texture, Color(0, 0, 0), pos,
			                       Texture_painter::SOLID, true);
		}

		Texture_base *alloc_texture(Area size, bool has_alpha) override
		{
			using namespace Genode;

			PT *pixel = (PT *)_alloc.alloc(size.count()*sizeof(PT));

			unsigned char *alpha = 0;

			if (has_alpha)
				alpha = (unsigned char *)_alloc.alloc(size.count());

			return new (_alloc) Genode::Texture<PT>(pixel, alpha, size);
		}

		void free_texture(Texture_base *texture_base) override
		{
			using namespace Genode;

			Texture<PT> *texture = static_cast<Texture<PT> *>(texture_base);

			Genode::size_t const num_pixels = texture->size().count();

			if (texture->alpha())
				_alloc.free(texture->alpha(), num_pixels);

			_alloc.free(texture->pixel(), sizeof(PT)*num_pixels);

			destroy(_alloc, texture);
		}

		void set_rgba_texture(Texture_base *texture_base,
		                      unsigned char const *rgba,
		                      unsigned len, int y) override
		{
			Texture<PT> *texture = static_cast<Texture<PT> *>(texture_base);

			texture->rgba(rgba, len, y);
		}
};

#endif /* _INCLUDE__SCOUT__CANVAS_H_ */
