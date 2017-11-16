/*
 * \brief  Texture allocated as RAM dataspace
 * \author Norman Feske
 * \date   2017-11-16
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CHUNKY_TEXTURE_H_
#define _CHUNKY_TEXTURE_H_

/* local includes */
#include "buffer.h"

namespace Nitpicker { template <typename> class Chunky_texture; }


template <typename PT>
class Nitpicker::Chunky_texture : public Buffer, public Texture<PT>
{
	private:

		Framebuffer::Mode::Format _format() {
			return Framebuffer::Mode::RGB565; }

		/**
		 * Return base address of alpha channel or 0 if no alpha channel exists
		 */
		unsigned char *_alpha_base(Area size, bool use_alpha)
		{
			if (!use_alpha) return 0;

			/* alpha values come right after the pixel values */
			return (unsigned char *)local_addr() + calc_num_bytes(size, false);
		}

	public:

		/**
		 * Constructor
		 */
		Chunky_texture(Ram_session &ram, Region_map &rm, Area size, bool use_alpha)
		:
			Buffer(ram, rm, size, _format(), calc_num_bytes(size, use_alpha)),
			Texture<PT>((PT *)local_addr(),
			            _alpha_base(size, use_alpha), size) { }

		static size_t calc_num_bytes(Area size, bool use_alpha)
		{
			/*
			 * If using an alpha channel, the alpha buffer follows the
			 * pixel buffer. The alpha buffer is followed by an input
			 * mask buffer. Hence, we have to account one byte per
			 * alpha value and one byte for the input mask value.
			 */
			size_t bytes_per_pixel = sizeof(PT) + (use_alpha ? 2 : 0);
			return bytes_per_pixel*size.w()*size.h();
		}

		unsigned char *input_mask_buffer()
		{
			if (!Texture<PT>::alpha()) return 0;

			Area const size = Texture<PT>::size();

			/* input-mask values come right after the alpha values */
			return (unsigned char *)local_addr() + calc_num_bytes(size, false)
			                                     + size.count();
		}
};

#endif /* _CHUNKY_TEXTURE_H_ */
