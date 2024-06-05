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
#include <util/interface.h>

namespace Genode {

	class Surface_base;
	template <typename> class Surface;
}


/*
 * A surface is a rectangular space to which drawing operations can be
 * applied. All coordinates are specified in pixels. The coordinate origin
 * is the top-left corner of the surface.
 */
class Genode::Surface_base : Interface
{
	private:

		/*
		 * Noncopyable
		 */
		Surface_base(Surface_base const &);
		Surface_base &operator = (Surface_base const &);

	public:

		using Rect  = Genode::Rect<>;
		using Point = Rect::Point;
		using Area  = Rect::Area;

		enum Pixel_format { UNKNOWN, RGB565, RGB888, ALPHA8 };

		struct Flusher : Interface
		{
			virtual void flush_pixels(Rect rect) = 0;
		};

	protected:

		Area         const _size;      /* boundaries of surface */
		Pixel_format const _format;

		Flusher *_flusher = nullptr;

		Rect _clip = Rect(Point(), _size);

		Surface_base(Area size, Pixel_format format)
		: _size(size), _format(format) { }

	public:

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
		void clip(Rect clip)
		{
			_clip = Rect::intersect(Rect(Point(), _size), clip);
		}

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
	private:

		/*
		 * Noncopyable
		 */
		Surface(Surface const &);
		Surface &operator = (Surface const &);

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
