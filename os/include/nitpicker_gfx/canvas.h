/*
 * \brief   Generic interface of graphics backend
 * \date    2006-08-04
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NITPICKER_GFX__CANVAS_H_
#define _INCLUDE__NITPICKER_GFX__CANVAS_H_

#include <util/geometry.h>

#include <nitpicker_gfx/color.h>
#include <nitpicker_gfx/font.h>


class Texture : public Genode::Area<>
{
	public:

		Texture(Area<> size): Area<>(size) { }

		virtual ~Texture() { }

		/**
		 * Return pointer to alpha values, or 0 if no alpha channel exists
		 */
		virtual unsigned char const *alpha() const { return 0; }
};


/*
 * A canvas is a rectangular surface to which drawing operations can be
 * applied. All coordinates are specified in pixels. The coordinate origin
 * is the top-left corner of the canvas.
 */
class Canvas
{
	public:

		typedef Genode::Point<> Point;
		typedef Genode::Area<>  Area;
		typedef Genode::Rect<>  Rect;

	protected:

		Rect _clip;      /* clipping area        */
		Area _size;      /* boundaries of canvas */

		/**
		 * Constructor
		 */
		Canvas(Area size) : _clip(Point(0, 0), size), _size(size) { }

		/**
		 * Register canvas area as to be flushed
		 *
		 * This function is called by the graphics primitives when
		 * canvas regions are changed.
		 */
		virtual void _flush_pixels(Rect) { }

	public:

		/*
		 * Modes for drawing textures
		 *
		 * The solid mode is used for normal operation in Nitpicker's
		 * flat mode and corresponds to plain pixel blitting. The
		 * masked mode allows us to tint texture with a specified
		 * mixing color. This feature is used by the X-Ray and Kill
		 * mode. The masked mode leaves all pixels untouched for
		 * which the corresponding texture pixel equals the mask key
		 * color (we use black). We use this mode for painting
		 * the mouse cursor.
		 */
		enum Mode {
			SOLID  = 0,   /* draw texture pixel              */
			MIXED  = 1,   /* mix texture pixel and color 1:1 */
			MASKED = 2,   /* skip pixels with mask color     */
		};

		virtual ~Canvas() { }

		/**
		 * Define/request clipping rectangle
		 */
		void clip(Rect clip) {
			_clip = Rect::intersect(Rect(Point(0, 0), _size), clip); }

		Rect clip()       const { return _clip; }
		bool clip_valid() const { return _clip.valid(); }

		/**
		 * Return dimension of canvas in pixels
		 */
		Area size() const { return _size; }

		/**
		 * Draw filled box
		 *
		 * \param rect   position and size of box.
		 * \param color  drawing color.
		 */
		virtual void draw_box(Rect rect, Color color) = 0;

		/**
		 * Draw string
		 */
		virtual void draw_string(Point position, Font const &font, Color color,
		                         const char *str) = 0;

		/**
		 * Draw texture
		 */
		virtual void draw_texture(Texture const &src, Color mix_color, Point position,
		                          Mode mode, bool allow_alpha = true) = 0;
};

#endif /* _INCLUDE__NITPICKER_GFX__CANVAS_H_ */
