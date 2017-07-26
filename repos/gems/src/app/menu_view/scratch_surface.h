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

namespace Menu_view { template <typename PT> class Scratch_surface; }


template <typename PT>
class Menu_view::Scratch_surface
{
	private:

		Area           _size;
		Allocator     &_alloc;
		unsigned char *_base = nullptr;
		size_t         _num_bytes = 0;

		size_t _needed_bytes(Area size)
		{
			/* account for pixel buffer and alpha channel */
			return size.count()*sizeof(PT) + size.count();
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
			return _base + _size.count()*sizeof(PT);
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

		Surface<PT> pixel_surface() const
		{
			return Surface<PT>((PT *)_pixel_base(), _size);
		}

		Surface<Pixel_alpha8> alpha_surface() const
		{
			return Surface<Pixel_alpha8>((Pixel_alpha8 *)_alpha_base(), _size);
		}

		Texture<PT> texture() const
		{
			return Texture<PT>((PT *)_pixel_base(), _alpha_base(), _size);
		}
};

#endif /* _SCRATCH_SURFACE_H_ */
