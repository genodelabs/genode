/*
 * \brief  Window decorator that can be styled - theme handling
 * \author Norman Feske
 * \date   2015-11-12
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/texture_rgb888.h>
#include <nitpicker_gfx/text_painter.h>
#include <util/xml_node.h>
#include <decorator/xml_utils.h>

/* gems includes */
#include <gems/file.h>
#include <gems/png_image.h>

/* demo includes */
#include <scout_gfx/icon_painter.h>

/* local includes */
#include "theme.h"


enum Texture_id { TEXTURE_ID_DEFAULT, TEXTURE_ID_CLOSER, TEXTURE_ID_MAXIMIZER };


struct Texture_from_png_file
{
	typedef Genode::Texture<Genode::Pixel_rgb888> Texture;

	File       png_file;
	Png_image  png_image { png_file.data<void>() };
	Texture   &texture { *png_image.texture<Genode::Pixel_rgb888>() };

	Texture_from_png_file(char const *path, Genode::Allocator &alloc)
	:
		png_file(path, alloc)
	{ }
};


static Genode::Texture<Genode::Pixel_rgb888> const &
texture_by_id(Texture_id texture_id, Genode::Allocator &alloc)
{
	if (texture_id == TEXTURE_ID_DEFAULT) {
		static Texture_from_png_file texture("theme/default.png", alloc);
		return texture.texture;
	}

	if (texture_id == TEXTURE_ID_CLOSER) {
		static Texture_from_png_file texture("theme/closer.png", alloc);
		return texture.texture;
	}

	if (texture_id == TEXTURE_ID_MAXIMIZER) {
		static Texture_from_png_file texture("theme/maximizer.png", alloc);
		return texture.texture;
	}

	struct Invalid_texture_id { };
	throw  Invalid_texture_id();
}


static Genode::Texture<Genode::Pixel_rgb888> const &
texture_by_element_type(Decorator::Theme::Element_type type, Genode::Allocator &alloc)
{
	switch (type) {
	case Decorator::Theme::ELEMENT_TYPE_CLOSER:
		return texture_by_id(TEXTURE_ID_CLOSER, alloc);

	case Decorator::Theme::ELEMENT_TYPE_MAXIMIZER:
		return texture_by_id(TEXTURE_ID_MAXIMIZER, alloc);
	}
	struct Invalid_element_type { };
	throw  Invalid_element_type();
};


static Text_painter::Font const &title_font(Genode::Allocator &alloc)
{
	static File tff_file("theme/font.tff", alloc);
	static Text_painter::Font font(tff_file.data<char>());

	return font;
}


static Genode::Xml_node metadata(Genode::Allocator &alloc)
{
	static File file("theme/metadata", alloc);

	return Genode::Xml_node(file.data<char>(), file.size());
}


Decorator::Area Decorator::Theme::background_size() const
{
	Genode::Texture<Pixel_rgb888> const &texture = texture_by_id(TEXTURE_ID_DEFAULT, _alloc);

	return texture.size();
}


struct Margins_from_metadata : Decorator::Theme::Margins
{
	Margins_from_metadata(char const *sub_node, Genode::Allocator &alloc)
	{
		Genode::Xml_node aura = metadata(alloc).sub_node(sub_node);
		top    = aura.attribute_value("top",    0UL);
		bottom = aura.attribute_value("bottom", 0UL);
		left   = aura.attribute_value("left",   0UL);
		right  = aura.attribute_value("right",  0UL);
	}
};


Decorator::Theme::Margins Decorator::Theme::aura_margins() const
{
	static Margins_from_metadata aura("aura", _alloc);
	return aura;
}


Decorator::Theme::Margins Decorator::Theme::decor_margins() const
{
	static Margins_from_metadata decor("decor", _alloc);
	return decor;
}


Decorator::Rect Decorator::Theme::title_geometry() const
{
	static Rect rect = rect_attribute(metadata(_alloc).sub_node("title"));
	return rect;
}


Decorator::Rect Decorator::Theme::element_geometry(Element_type type) const
{
	if (type == ELEMENT_TYPE_CLOSER) {
		static Rect rect(point_attribute(metadata(_alloc).sub_node("closer")),
		                 texture_by_id(TEXTURE_ID_CLOSER, _alloc).size());
		return rect;
	}

	if (type == ELEMENT_TYPE_MAXIMIZER) {
		static Rect rect(point_attribute(metadata(_alloc).sub_node("maximizer")),
		                 texture_by_id(TEXTURE_ID_MAXIMIZER, _alloc).size());
		return rect;
	}

	struct Invalid_element_type { };
	throw  Invalid_element_type();
}


void Decorator::Theme::draw_background(Decorator::Pixel_surface pixel_surface,
                                       Decorator::Alpha_surface alpha_surface,
                                       unsigned alpha) const
{
	Genode::Texture<Pixel_rgb888> const &texture = texture_by_id(TEXTURE_ID_DEFAULT, _alloc);

	typedef Genode::Surface_base::Point Point;
	typedef Genode::Surface_base::Rect  Rect;

	unsigned const left  = aura_margins().left  + decor_margins().left;
	unsigned const right = aura_margins().right + decor_margins().right;

	unsigned const middle = left + right < pixel_surface.size().w()
	                      ? pixel_surface.size().w() - left - right
	                      : 0;

	Rect const orig_clip = pixel_surface.clip();

	/* left */
	if (left) {
		Rect curr_clip = Rect(Point(0, 0), Area(left, pixel_surface.size().h()));
		pixel_surface.clip(curr_clip);
		alpha_surface.clip(curr_clip);

		Rect const rect(Point(0, 0), pixel_surface.size());

		Icon_painter::paint(pixel_surface, rect, texture, alpha);
		Icon_painter::paint(alpha_surface, rect, texture, alpha);
	}

	/* middle */
	if (middle) {
		Rect curr_clip = Rect(Point(left, 0), Area(middle, pixel_surface.size().h()));
		pixel_surface.clip(curr_clip);
		alpha_surface.clip(curr_clip);

		Rect const rect(Point(0, 0), pixel_surface.size());

		Icon_painter::paint(pixel_surface, rect, texture, alpha);
		Icon_painter::paint(alpha_surface, rect, texture, alpha);
	}

	/* right */
	if (right) {
		Rect curr_clip = Rect(Point(left + middle, 0), Area(right, pixel_surface.size().h()));
		pixel_surface.clip(curr_clip);
		alpha_surface.clip(curr_clip);

		Point at(0, 0);
		Area size = pixel_surface.size();

		if (texture.size().w() > pixel_surface.size().w()) {
			at = Point((int)pixel_surface.size().w() - (int)texture.size().w(), 0);
			size = Area(texture.size().w(), size.h());
		}

		Icon_painter::paint(pixel_surface, Rect(at, size), texture, alpha);
		Icon_painter::paint(alpha_surface, Rect(at, size), texture, alpha);
	}

	pixel_surface.clip(orig_clip);
}


void Decorator::Theme::draw_title(Decorator::Pixel_surface pixel_surface,
                                  Decorator::Alpha_surface alpha_surface,
                                  char const *title) const
{
	Text_painter::Font const &font = title_font(_alloc);

	Area  const label_area(font.str_w(title), font.str_h(title));
	Rect  const surface_rect(Point(0, 0), pixel_surface.size());
	Rect  const title_rect = absolute(title_geometry(), surface_rect);
	Point const centered_text_pos = title_rect.center(label_area) - Point(0, 1);

	Text_painter::paint(pixel_surface, centered_text_pos, font,
	                    Genode::Color(0, 0, 0), title);
}


void Decorator::Theme::draw_element(Decorator::Pixel_surface pixel_surface,
                                    Decorator::Alpha_surface alpha_surface,
                                    Element_type element_type,
                                    unsigned alpha) const
{
	Genode::Texture<Pixel_rgb888> const &texture =
		texture_by_element_type(element_type, _alloc);

	Rect  const surface_rect(Point(0, 0), pixel_surface.size());
	Rect  const element_rect = element_geometry(element_type);
	Point const pos = absolute(element_rect.p1(), surface_rect);
	Rect  const rect(pos, element_rect.area());

	Icon_painter::paint(pixel_surface, rect, texture, alpha);
	Icon_painter::paint(alpha_surface, rect, texture, alpha);
}


Decorator::Point Decorator::Theme::absolute(Decorator::Point pos,
                                            Decorator::Rect win_rect) const
{
	Area const theme_size = background_size();

	int x = pos.x();
	int y = pos.y();

	if (x > (int)theme_size.w()/2) x = win_rect.w() - theme_size.w() + x;
	if (y > (int)theme_size.h()/2) y = win_rect.h() - theme_size.h() + y;

	return win_rect.p1() + Point(x, y);
}


Decorator::Rect Decorator::Theme::absolute(Decorator::Rect rect,
                                           Decorator::Rect win_rect) const
{
	return Rect(absolute(rect.p1(), win_rect), absolute(rect.p2(), win_rect));
}
