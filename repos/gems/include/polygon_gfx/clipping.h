/*
 * \brief   Polygon clipping
 * \date    2015-06-19
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__POLYGON_GFX__CLIPPING_H_
#define _INCLUDE__POLYGON_GFX__CLIPPING_H_

namespace Polygon {

	struct Point_base;

	inline int intersect_ratio(int, int, int);

	template <typename> struct Clipper_vertical;
	template <typename> struct Clipper_horizontal;

	struct Clipper_min;
	struct Clipper_max;

	template <typename, typename, typename> struct Clipper;

	template <typename> struct Clipper_2d;
}


/**
 * Common base class of polygon points
 */
struct Polygon::Point_base : Genode::Point<>
{
	/*
	 * Number of attributes to interpolate along the polygon edges. This value
	 * must be defined by each derived class.
	 */
	enum { NUM_EDGE_ATTRIBUTES = 1 };

	/**
	 * Constructors
	 */
	Point_base() { }
	Point_base(int x, int y): Genode::Point<>(x, y) { }

	/**
	 * Return edge attribute by ID
	 */
	inline int edge_attr(int id) const { return x(); }

	/**
	 * Assign value to edge attribute with specified ID
	 */
	inline void edge_attr(int id, int value) { *this = Point_base(value, y()); }
};


/**
 * Calculate ratio of range intersection
 *
 * \param v_start   Start of range
 * \param v_end     End of range
 * \param v_cut     Range cut (should be in interval v_start...v_end)
 * \return          Ratio of intersection as 16.16 fixpoint
 *
 * The input arguments 'v_start', 'v_end', 'v_cut' must use only
 * the lower 16 bits of the int type.
 */
inline int Polygon::intersect_ratio(int v_start, int v_end, int v_cut)
{
	int dv     = v_end - v_start,
	    dv_cut = v_cut - v_start;

	return dv ? (dv_cut<<16)/dv : 0;
}


/**
 * Support for vertical clipping boundary
 */
template <typename POINT>
struct Polygon::Clipper_vertical
{
	/**
	 * Select clipping-sensitive attribute from polygon point
	 */
	static int clip_value(POINT p) { return p.x(); }

	/**
	 * Calculate intersection point
	 */
	static POINT clip(POINT p1, POINT p2, int clip)
	{
		/*
		 * Enforce unique x order of points to apply rounding errors
		 * consistently also when edge points are specified in reverse.
		 * Typically the same edge is used in reverse direction by
		 * each neighboured polygon.
		 */
		if (clip_value(p1) > clip_value(p2)) { POINT tmp = p1; p1 = p2; p2 = tmp; }

		/* calculate ratio of the intersection of edge and clipping boundary */
		int ratio = intersect_ratio(p1.x(), p2.x(), clip);

		/* calculate y value at intersection point */
		POINT result;
		*(Point_base *)&result = Point_base(clip, p1.y() + ((ratio*(p2.y() - p1.y()))>>16));

		/* calculate intersection values for edge attributes other than x */
		for (int i = 1; i < POINT::NUM_EDGE_ATTRIBUTES; i++) {
			int v1 = p1.edge_attr(i),
			    v2 = p2.edge_attr(i);
			result.edge_attr(i, v1 + ((ratio*(v2 - v1))>>16));
		}
		return result;
	}
};


/**
 * Support for horizontal clipping boundary
 */
template <typename POINT>
struct Polygon::Clipper_horizontal
{
	/**
	 * Select clipping-sensitive attribute from polygon point
	 */
	static int clip_value(POINT p) { return p.y(); }

	/**
	 * Calculate intersection point
	 */
	static POINT clip(POINT p1, POINT p2, int clip)
	{
		if (clip_value(p1) > clip_value(p2)) { POINT tmp = p1; p1 = p2; p2 = tmp; }

		/* calculate ratio of the intersection of edge and clipping boundary */
		int ratio = intersect_ratio(clip_value(p1), clip_value(p2), clip);

		/* calculate y value at intersection point */
		POINT result;
		*(Point_base *)&result = Point_base(p1.x() + ((ratio*(p2.x() - p1.x()))>>16), clip);

		/* calculate intersection values for edge attributes other than x */
		for (int i = 1; i < POINT::NUM_EDGE_ATTRIBUTES; i++) {
			int v1 = p1.edge_attr(i),
			    v2 = p2.edge_attr(i);
			result.edge_attr(i, v1 + ((ratio*(v2 - v1))>>16));
		}
		return result;
	}
};


/**
 * Support for clipping against a lower boundary
 */
struct Polygon::Clipper_min
{
	static bool inside(int value, int boundary) { return value >= boundary; }
};


/**
 * Support for clipping against a higher boundary
 */
struct Polygon::Clipper_max
{
	static bool inside(int value, int boundary) { return value <= boundary; }
};


/**
 * One-dimensional clipping
 *
 * This template allows for the aggregation of the policies defined above to
 * build specialized 1D-clipping functions for upper/lower vertical/horizontal
 * clipping boundaries and for polygon points with different attributes.
 */
template <typename CLIPPER_DIRECTION, typename CLIPPER_MINMAX, typename POINT>
struct Polygon::Clipper : CLIPPER_DIRECTION, CLIPPER_MINMAX
{
	/**
	 * Check whether point is inside the clipping area or not
	 */
	static bool inside(POINT p, int clip)
	{
		return CLIPPER_MINMAX::inside(CLIPPER_DIRECTION::clip_value(p), clip);
	}
};


/**
 * Create clipping helpers
 *
 * This class is used as a compound containing all rules to
 * clip a polygon against a 2d region such as the clipping
 * region of a Canvas.
 */
template <typename POINT>
struct Polygon::Clipper_2d
{
	typedef Clipper<Clipper_horizontal<POINT>, Clipper_min, POINT> Top;
	typedef Clipper<Clipper_horizontal<POINT>, Clipper_max, POINT> Bottom;
	typedef Clipper<Clipper_vertical<POINT>,   Clipper_min, POINT> Left;
	typedef Clipper<Clipper_vertical<POINT>,   Clipper_max, POINT> Right;
};

#endif /* _INCLUDE__POLYGON_GFX__CLIPPING_H_ */
