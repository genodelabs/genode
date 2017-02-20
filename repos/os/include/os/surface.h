/*
 * \brief   Generic interface to a graphics backend
 * \date    2006-08-04
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__SURFACE_H_
#define _INCLUDE__OS__SURFACE_H_

#include <util/geometry.h>
#include <util/color.h>

namespace Genode {

	class Surface_base;
	template <typename> class Surface;
}


/*
 * A surface is a rectangular space to which drawing operations can be
 * applied. All coordinates are specified in pixels. The coordinate origin
 * is the top-left corner of the surface.
 */
class Genode::Surface_base
{
	public:

		typedef Genode::Point<> Point;
		typedef Genode::Area<>  Area;
		typedef Genode::Rect<>  Rect;
		typedef Genode::Color   Color;

		enum Pixel_format { UNKNOWN, RGB565, RGB888, ALPHA8 };

		struct Flusher
		{
			virtual void flush_pixels(Rect rect) = 0;
		};

	protected:

		Rect         _clip;      /* clipping area         */
		Area         _size;      /* boundaries of surface */
		Pixel_format _format;
		Flusher     *_flusher;

		/**
		 * Constructor
		 */
		Surface_base(Area size, Pixel_format format)
		: _clip(Point(0, 0), size), _size(size), _format(format), _flusher(0) { }

	public:

		virtual ~Surface_base() { }

		/**
		 * Register part of surface to be flushed
		 *
		 * This method is called by graphics primitives when surface regions
		 * are changed.
		 */
		void flush_pixels(Rect rect)
		{
			if (_flusher)
				_flusher->flush_pixels(rect);
		}

		/**
		 * Register pixel flusher
		 */
		void flusher(Flusher *flusher) { _flusher = flusher; }

		/**
		 * Define/request clipping rectangle
		 */
		void clip(Rect clip) {
			_clip = Rect::intersect(Rect(Point(0, 0), _size), clip); }

		Rect clip()       const { return _clip; }
		bool clip_valid() const { return _clip.valid(); }

		Pixel_format pixel_format() const { return _format; }

		/**
		 * Return dimension of surface in pixels
		 */
		Area size() const { return _size; }
};


/**
 * Surface that stores each pixel in one storage unit in a linear buffer
 *
 * \param PT  pixel type
 */
template <typename PT>
class Genode::Surface : public Surface_base
{
	protected:

		PT *_addr;   /* base address of pixel buffer   */

	public:

		PT *addr() { return _addr; }

		/**
		 * Constructor
		 */
		Surface(PT *addr, Area size)
		: Surface_base(size, PT::format()), _addr(addr) { }
};

#endif /* _INCLUDE__OS__SURFACE_H_ */
