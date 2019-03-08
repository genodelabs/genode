/*
 * \brief  Utility for off-screen rendering of widget elements
 * \author Norman Feske
 * \date   2009-09-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SCRATCH_SURFACE_H_
#define _SCRATCH_SURFACE_H_

/* local includes */
#include <types.h>

namespace Menu_view {

	struct Additive_alpha;
	struct Opaque_pixel;

	class Scratch_surface;
}


/**
 * Custom pixel type for applying painters to an alpha channel
 *
 * The 'transfer' function of this pixel type applies alpha channel values
 * from textures to it's 'pixel' in an additive way. It is designated for
 * blending alpha channels from different textures together.
 */
struct Menu_view::Additive_alpha
{
	uint8_t pixel;

	template <typename TPT, typename PT>
	static void transfer(TPT const &, int src_a, int alpha, PT &dst)
	{
		dst.pixel += (alpha*src_a) >> 8;
	}

	static Surface_base::Pixel_format format() { return Surface_base::UNKNOWN; }

} __attribute__((packed));


/**
 * Custom pixel type to apply painters w/o the texture's alpha channel
 *
 * This pixel type is useful for limiting the application of painters to color
 * values only. It allows for the blending of a texture's color channels
 * independent from the texture's alpha channel.
 */
struct Menu_view::Opaque_pixel : Pixel_rgb888
{
	template <typename TPT, typename PT>
	static void transfer(TPT const &src, int, int alpha, PT &dst)
	{
		if (alpha) dst.pixel = PT::mix(dst, src, alpha).pixel;
	}

	static Surface_base::Pixel_format format() { return Surface_base::UNKNOWN; }

} __attribute__((packed));


class Menu_view::Scratch_surface
{
	private:

		/**
		 * Noncopyable
		 */
		Scratch_surface(Scratch_surface const &);
		Scratch_surface &operator = (Scratch_surface const &);

		Area           _size { };
		Allocator     &_alloc;
		unsigned char *_base = nullptr;
		size_t         _num_bytes = 0;

		size_t _needed_bytes(Area size)
		{
			/* account for pixel buffer and alpha channel */
			return size.count()*sizeof(Opaque_pixel)
			     + size.count()*sizeof(Additive_alpha);
		}

		void _release()
		{
			if (_base) {
				_alloc.free(_base, _num_bytes);
				_base = nullptr;
			}
		}

		unsigned char *_pixel_base() const { return _base; }

		unsigned char *_alpha_base() const
		{
			return _base + _size.count()*sizeof(Opaque_pixel);
		}

	public:

		Scratch_surface(Allocator &alloc) : _alloc(alloc) { }

		~Scratch_surface()
		{
			_release();
		}

		void reset(Area size)
		{
			if (_num_bytes < _needed_bytes(size)) {
				_release();

				_size      = size;
				_num_bytes = _needed_bytes(size);
				_base      = (unsigned char *)_alloc.alloc(_num_bytes);
			}

			Genode::memset(_base, 0, _num_bytes);
		}

		template <typename FN>
		void apply(FN const &fn)
		{
			Surface<Opaque_pixel>   pixel((Opaque_pixel   *)_pixel_base(), _size);
			Surface<Additive_alpha> alpha((Additive_alpha *)_alpha_base(), _size);
			fn(pixel, alpha);
		}

		Texture<Pixel_rgb888> texture() const
		{
			return Texture<Pixel_rgb888>((Pixel_rgb888 *)_pixel_base(), _alpha_base(), _size);
		}
};

#endif /* _SCRATCH_SURFACE_H_ */
