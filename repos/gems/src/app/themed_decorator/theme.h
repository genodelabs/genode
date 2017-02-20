/*
 * \brief  Window decorator that can be styled - theme handling
 * \author Norman Feske
 * \date   2015-11-12
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _THEME_H_
#define _THEME_H_

/* Genode includes */
#include <os/texture.h>
#include <os/pixel_rgb565.h>
#include <os/pixel_alpha8.h>
#include <os/pixel_rgb888.h>

namespace Decorator {

	class Theme;

	typedef Genode::Pixel_rgb888 Pixel_rgb888;
	typedef Genode::Pixel_rgb565 Pixel_rgb565;
	typedef Genode::Pixel_alpha8 Pixel_alpha8;

	typedef Genode::Surface<Pixel_rgb888> Pixel_surface;
	typedef Genode::Surface<Pixel_alpha8> Alpha_surface;

	typedef Genode::Surface_base::Area  Area;
	typedef Genode::Surface_base::Point Point;
	typedef Genode::Surface_base::Rect  Rect;
}


class Decorator::Theme
{
	private:

		Genode::Ram_session &_ram;
		Genode::Region_map  &_rm;
		Genode::Allocator   &_alloc;

	public:

		struct Margins
		{
			unsigned top, bottom, left, right;

			bool none() const { return !top && !bottom && !left && !right; }
		};

		enum Element_type { ELEMENT_TYPE_CLOSER, ELEMENT_TYPE_MAXIMIZER };

		Theme(Genode::Ram_session &ram, Genode::Region_map &rm, Genode::Allocator &alloc)
		: _ram(ram), _rm(rm), _alloc(alloc) { }

		Area background_size() const;

		Margins aura_margins() const;

		Margins decor_margins() const;

		void draw_background(Pixel_surface, Alpha_surface, unsigned alpha) const;

		void draw_title(Pixel_surface, Alpha_surface, char const *title) const;

		void draw_element(Pixel_surface, Alpha_surface, Element_type,
		                  unsigned alpha) const;

		/**
		 * Return geometry of theme elements in the theme coordinate space
		 */
		Rect title_geometry() const;
		Rect element_geometry(Element_type) const;

		/**
		 * Calculate screen-absolute coordinate for a position within the theme
		 * coordinate space
		 */
		Point absolute(Point theme_pos, Rect win_outer_geometry) const;

		Rect absolute(Rect theme_rect, Rect win_outer_geometry) const;
};

#endif /* _THEME_H_ */
