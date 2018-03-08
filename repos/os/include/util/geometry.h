/*
 * \brief  Geometric primitives
 * \author Norman Feske
 * \date   2006-08-05
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__GEOMETRY_H_
#define _INCLUDE__UTIL__GEOMETRY_H_

#include <util/misc_math.h>
#include <base/stdint.h>
#include <base/output.h>

namespace Genode {
	template <typename CT = int>                         class Point;
	template <typename DT = unsigned>                    class Area;
	template <typename CT = int, typename DT = unsigned> class Rect;
}


/**
 * \param CT  coordinate type
 */
template <typename CT>
class Genode::Point
{
	private:

		CT _x, _y;

	public:

		Point(CT x, CT y): _x(x), _y(y) { }
		Point(): _x(0), _y(0) { }

		CT x() const { return _x; }
		CT y() const { return _y; }

		/**
		 * Operator for adding points
		 */
		Point operator + (Point const &p) const { return Point(_x + p.x(), _y + p.y()); }

		/**
		 * Operator for subtracting points
		 */
		Point operator - (Point const &p) const { return Point(_x - p.x(), _y - p.y()); }

		/**
		 * Operator for testing non-equality of two points
		 */
		bool operator != (Point const &p) const { return p.x() != _x || p.y() != _y; }

		/**
		 * Operator for testing equality of two points
		 */
		bool operator == (Point const &p) const { return p.x() == _x && p.y() == _y; }

		void print(Output &out) const
		{
			Genode::print(out, _x >= 0 ? "+" : "-", abs(_x),
			                   _y >= 0 ? "+" : "-", abs(_y));
		}
};


/**
 * \param DT  distance type
 */
template <typename DT>
class Genode::Area
{
	private:

		DT _w, _h;

	public:

		Area(DT w, DT h): _w(w), _h(h) { }
		Area(): _w(0), _h(0) { }

		DT w() const { return _w; }
		DT h() const { return _h; }

		bool valid() const { return _w > 0 && _h > 0; }

		size_t count() const { return _w*_h; }

		/**
		 * Operator for testing non-equality of two areas
		 */
		bool operator != (Area const &a) const { return a.w() != _w || a.h() != _h; }

		/**
		 * Operator for testing equality of two areas
		 */
		bool operator == (Area const &a) const { return a.w() == _w && a.h() == _h; }

		void print(Output &out) const { Genode::print(out, _w, "x", _h); }
};


/**
 * Rectangle
 *
 * A valid rectangle consists of two points wheras point 2 has higher or equal
 * coordinates than point 1. All other cases are threated as invalid
 * rectangles.
 *
 * \param CT  coordinate type
 * \param DT  distance type
 */
template <typename CT, typename DT>
class Genode::Rect
{
	private:

		Point<CT> _p1, _p2;

	public:

		/**
		 * Constructors
		 */
		Rect(Point<CT> p1, Point<CT> p2): _p1(p1), _p2(p2) { }

		Rect(Point<CT> p, Area<DT> a)
		: _p1(p), _p2(p.x() + a.w() - 1, p.y() + a.h() - 1) { }

		Rect() : /* invalid */ _p1(1, 1), _p2(0, 0) { }

		/**
		 * Accessors
		 */
		CT        x1()   const { return _p1.x(); }
		CT        y1()   const { return _p1.y(); }
		CT        x2()   const { return _p2.x(); }
		CT        y2()   const { return _p2.y(); }
		DT        w()    const { return _p2.x() - _p1.x() + 1; }
		DT        h()    const { return _p2.y() - _p1.y() + 1; }
		Point<CT> p1()   const { return _p1; }
		Point<CT> p2()   const { return _p2; }
		Area<DT>  area() const { return Area<DT>(w(), h()); }

		/**
		 * Return true if rectangle area is greater than zero
		 */
		bool valid() const { return _p1.x() <= _p2.x() && _p1.y() <= _p2.y(); }

		/**
		 * Return true if area fits in rectangle
		 */
		bool fits(Area<DT> area) const { return w() >= area.w() && h() >= area.h(); }

		/**
		 * Return true if the specified point lies within the rectangle
		 */
		bool contains(Point<CT> p) const {
			return p.x() >= x1() && p.x() <= x2() && p.y() >= y1() && p.y() <= y2(); }

		/**
		 * Create new rectangle by intersecting two rectangles
		 */
		static Rect intersect(Rect r1, Rect r2) {
			return Rect(Point<CT>(max(r1.x1(), r2.x1()), max(r1.y1(), r2.y1())),
			            Point<CT>(min(r1.x2(), r2.x2()), min(r1.y2(), r2.y2()))); }

		/**
		 * Compute compounding rectangle of two rectangles
		 */
		static Rect compound(Rect r1, Rect r2) {
			return Rect(Point<CT>(min(r1.x1(), r2.x1()), min(r1.y1(), r2.y1())),
			            Point<CT>(max(r1.x2(), r2.x2()), max(r1.y2(), r2.y2()))); }

		/**
		 * Cut out rectangle from rectangle
		 *
		 * \param r  rectangle to cut out
		 *
		 * In the worst case (if we cut a hole into the rectangle) we get
		 * four valid resulting rectangles.
		 */
		void cut(Rect r, Rect *top, Rect *left, Rect *right, Rect *bottom) const
		{
			/* limit the cut-out area to the actual rectangle */
			r = intersect(r, *this);

			*top    = Rect(Point<CT>(x1(), y1()),         Point<CT>(x2(), r.y1() - 1));
			*left   = Rect(Point<CT>(x1(), r.y1()),       Point<CT>(r.x1() - 1, r.y2()));
			*right  = Rect(Point<CT>(r.x2() + 1, r.y1()), Point<CT>(x2(), r.y2()));
			*bottom = Rect(Point<CT>(x1(), r.y2() + 1),   Point<CT>(x2(), y2()));
		}

		/**
		 * Return position of an area when centered within the rectangle
		 */
		Point<CT> center(Area<DT> area) const {
			return Point<CT>(((CT)w() - (CT)area.w())/2,
			                 ((CT)h() - (CT)area.h())/2) + p1(); }

		/**
		 * Print rectangle coordinates
		 *
		 * The output has the form 'width' x 'height' +/- 'p1.x' +/- 'p1.y'.
		 * For example, a rectange of size 15x16 as position (-13, 14) is
		 * printed as "15x16-13+14"
		 */
		void print(Output &out) const { Genode::print(out, area(), p1()); }
};

#endif /* _INCLUDE__UTIL__GEOMETRY_H_ */
