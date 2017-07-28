/*
 * \brief   Utilities for calculating bezier curves
 * \date    2017-07-28
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__BEZIER_H_
#define _INCLUDE__GEMS__BEZIER_H_

/**
 * Calculate quadratic bezier curve
 *
 * \param draw_line  functor to be called to draw a line segment
 * \param levels     number of sub divisions
 *
 * The coordinates are specified in clock-wise order with point 1 being
 * the start and point 3 the end of the curve.
 */
template <typename FUNC>
static inline void bezier(long x1, long y1, long x2, long y2, long x3, long y3,
                          FUNC const &draw_line, unsigned levels)
{
	if (levels-- == 0) {
		draw_line(x1, y1, x3, y3);
		return;
	}

	long const x12  =  (x1 +  x2) / 2,    y12  =  (y1 +  y2) / 2,
	           x23  =  (x2 +  x3) / 2,    y23  =  (y2 +  y3) / 2,
	           x123 = (x12 + x23) / 2,    y123 = (y12 + y23) / 2;

	bezier(x1, y1, x12, y12, x123, y123, draw_line, levels);
	bezier(x123, y123, x23, y23, x3, y3, draw_line, levels);
}


/**
 * Calculate cubic bezier curve
 *
 * The arguments correspond to those of the quadratic version but with point 4
 * being the end of the curve.
 */
template <typename FUNC>
static inline void bezier(long x1, long y1, long x2, long y2,
                          long x3, long y3, long x4, long y4,
                          FUNC const &draw_line, unsigned levels)
{
	if (levels-- == 0) {
		draw_line(x1, y1, x4, y4);
		return;
	}

	long const x12   =   (x1 +   x2) / 2,    y12   =   (y1 +   y2) / 2,
	           x23   =   (x2 +   x3) / 2,    y23   =   (y2 +   y3) / 2,
	           x34   =   (x3 +   x4) / 2,    y34   =   (y3 +   y4) / 2,
	           x123  =  (x12 +  x23) / 2,    y123  =  (y12 +  y23) / 2,
	           x234  =  (x23 +  x34) / 2,    y234  =  (y23 +  y34) / 2,
	           x1234 = (x123 + x234) / 2,    y1234 = (y123 + y234) / 2;

	bezier(x1, y1, x12, y12, x123, y123, x1234, y1234, draw_line, levels);
	bezier(x1234, y1234, x234, y234, x34, y34, x4, y4, draw_line, levels);
}

#endif /* _INCLUDE__GEMS__BEZIER_H_ */
