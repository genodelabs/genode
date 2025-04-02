/*
 * \brief  Blit API
 * \author Norman Feske
 * \date   2025-01-16
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__X86_64__BLIT_H_
#define _INCLUDE__SPEC__X86_64__BLIT_H_

#include <blit/types.h>
#include <blit/internal/sse4.h>
#include <blit/internal/slow.h>

namespace Blit {

	static inline void back2front(Surface<Pixel_rgb888> &surface,
	                        Texture<Pixel_rgb888> const &texture,
	                        Rect rect, Rotate rotate, Flip flip)
	{
		if (divisable_by_8x8(texture.size()))
			_b2f<Sse4>(surface, texture, rect, rotate, flip);
		else
			_b2f<Slow>(surface, texture, rect, rotate, flip);
	}

	static inline void blend_xrgb_a(auto &&... args) { Sse4::Blend::xrgb_a(args...); }
}

#endif /* _INCLUDE__SPEC__X86_64__BLIT_H_ */
