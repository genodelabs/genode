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

	struct Surface_window { unsigned y, h; };
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

		enum Pixel_format { UNKNOWN, RGB565, RGB888, ALPHA8, INPUT8 };

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

		static Area _sanitized(Area area, size_t const num_bytes)
		{
			/* prevent division by zero */
			if (area.w == 0)
				return { };

			size_t const bytes_per_line = area.w*sizeof(PT);

			return { .w = area.w,
			         .h = unsigned(min(num_bytes/bytes_per_line, area.h)) };
		}

	public:

		PT *addr() { return _addr; }

		/*
		 * \deprecated
		 */
		Surface(PT *addr, Area size) : Surface_base(size, PT::format()), _addr(addr) { }

		Surface(Byte_range_ptr const &bytes, Area const area)
		:
			Surface_base(_sanitized(area, bytes.num_bytes), PT::format()),
			_addr((PT *)bytes.start)
		{ }

		/**
		 * Call 'fn' with a sub-window surface as argument
		 *
		 * This method is useful for managing multiple surfaces within one
		 * larger surface, for example for organizing a back buffer and a front
		 * buffer within one virtual framebuffer.
		 */
		void with_window(Surface_window const win, auto const &fn) const
		{
			/* clip window coordinates against surface boundaries */
			Rect const rect = Rect::intersect({ { 0, 0 },          { 1, size().h } },
			                                  { { 0, int(win.y) }, { 1, win.h    } });

			Surface surface { _addr + rect.y1()*size().w, { size().w, rect.h() } };
			fn(surface);
		}
};

#endif /* _INCLUDE__OS__SURFACE_H_ */
