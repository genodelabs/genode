/*
 * \brief  Clipping guard
 * \author Norman Feske
 * \date   2006-08-08
 *
 * When drawing views recursively, we need to successively shrink the clipping
 * area to the intersection of the existing clipping area and the area of the
 * current view. After each drawing operation, we want to restore the previous
 * clipping area. The clipping guard functions the same way as a lock guard.
 * The clipping area of the canvas that is specified at the constructor of the
 * clipping guard gets shrinked as long as the clip guard exists. When we leave
 * the drawing function, all local variables including the clipping guard gets
 * destroyed and the clipping guard's destructor resets the clipping area of
 * the canvas.
 *
 * This mechanism effectively replaces the explicit clipping stack of the
 * original Nitpicker version by folding the clipping stack into the normal
 * stack.
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CLIP_GUARD_H_
#define _CLIP_GUARD_H_

#include "canvas.h"

class Clip_guard
{
	private:

		Canvas_base &_canvas;

		Rect const _orig_clip_rect;

	public:

		Clip_guard(Canvas_base &canvas, Rect new_clip_rect)
		:
			_canvas(canvas),
			_orig_clip_rect(_canvas.clip())
		{
			_canvas.clip(Rect::intersect(_orig_clip_rect, new_clip_rect));
		}

		~Clip_guard() { _canvas.clip(_orig_clip_rect); }
};

#endif /* _CLIP_GUARD_H_ */
