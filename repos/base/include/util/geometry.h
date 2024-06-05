/*
 * \brief  Geometric primitives
 * \author Norman Feske
 * \date   2006-08-05
 */

/*
 * Copyright (C) 2006-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__GEOMETRY_H_
#define _INCLUDE__UTIL__GEOMETRY_H_

#include <util/misc_math.h>
#include <util/xml_node.h>
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
struct Genode::Point
{
	CT x {}, y {};

	/**
	 * Operator for adding points
	 */
	Point operator + (Point const &p) const { return Point(x + p.x, y + p.y); }

	/**
	 * Operator for subtracting points
	 */
	Point operator - (Point const &p) const { return Point(x - p.x, y - p.y); }

	/**
	 * Operator for testing non-equality of two points
	 */
	bool operator != (Point const &p) const { return p.x != x || p.y != y; }

	/**
	 * Operator for testing equality of two points
	 */
	bool operator == (Point const &p) const { return p.x == x && p.y == y; }

	void print(Output &out) const
	{
		auto abs = [] (auto v) { return v >= 0 ? v : -v; };

		Genode::print(out, x >= 0 ? "+" : "-", abs(x),
		                   y >= 0 ? "+" : "-", abs(y));
	}

	/**
	 * Construct point from XML node attributes
	 *
	 * The XML node is expected to feature the attributes 'xpos' and 'ypos'.
	 */
	static Point from_xml(Xml_node const &node)
	{
		return Point(node.attribute_value("xpos", CT{}),
		             node.attribute_value("ypos", CT{}));
	}
};


/**
 * \param DT  distance type
 */
template <typename DT>
struct Genode::Area
{
	DT w {}, h {};

	bool valid() const { return w > 0 && h > 0; }

	size_t count() const { return size_t(w)*size_t(h); }

	/**
	 * Operator for testing non-equality of two areas
	 */
	bool operator != (Area const &a) const { return a.w != w || a.h != h; }

	/**
	 * Operator for testing equality of two areas
	 */
	bool operator == (Area const &a) const { return a.w == w && a.h == h; }

	void print(Output &out) const { Genode::print(out, w, "x", h); }

	/**
	 * Construct area from XML node attributes
	 *
	 * The XML node is expected to feature the attributes 'width' and
	 * 'height'.
	 */
	static Area from_xml(Xml_node const &node)
	{
		return Area(node.attribute_value("width",  DT{}),
		            node.attribute_value("height", DT{}));
	}
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
struct Genode::Rect
{
	using Point = Genode::Point<CT>;
	using Area  = Genode::Area<DT>;

	Point at   {};
	Area  area {};

	/**
	 * Construct rectangle from two given points
	 *
	 * The x and y coordinates of p1 must not be higher than those of p2.
	 */
	static constexpr Rect compound(Point const p1, Point const p2)
	{
		if (p1.x > p2.x || p1.y > p2.y) return { /* invalid */ };

		return { .at   = p1,
		         .area = { .w = DT(p2.x - p1.x + 1),
		                   .h = DT(p2.y - p1.y + 1) } };
	}

	/**
	 * Construct compounding rectangle of two rectangles
	 */
	static constexpr Rect compound(Rect r1, Rect r2)
	{
		return compound(Point(min(r1.x1(), r2.x1()), min(r1.y1(), r2.y1())),
		                Point(max(r1.x2(), r2.x2()), max(r1.y2(), r2.y2())));
	}

	/**
	 * Construct rectangle by intersecting two rectangles
	 */
	static constexpr Rect intersect(Rect const r1, Rect const r2)
	{
		return Rect::compound(Point(max(r1.x1(), r2.x1()), max(r1.y1(), r2.y1())),
		                      Point(min(r1.x2(), r2.x2()), min(r1.y2(), r2.y2())));
	}

	CT    x1() const { return at.x; }
	CT    y1() const { return at.y; }
	CT    x2() const { return at.x + area.w - 1; }
	CT    y2() const { return at.y + area.h - 1; }
	DT    w()  const { return area.w; }
	DT    h()  const { return area.h; }
	Point p1() const { return at; }
	Point p2() const { return { x2(), y2() }; }

	/**
	 * Return true if rectangle area is greater than zero
	 */
	bool valid() const { return area.valid(); }

	/**
	 * Return true if area fits in rectangle
	 */
	bool fits(Area const area) const { return w() >= area.w && h() >= area.h; }

	/**
	 * Return true if the specified point lies within the rectangle
	 */
	bool contains(Point const p) const
	{
		return p.x >= x1() && p.x <= x2() && p.y >= y1() && p.y <= y2();
	}

	struct Cut_remainder
	{
		Rect top, left, right, bottom;

		void for_each(auto const &fn) const { fn(top); fn(left); fn(right); fn(bottom); }
	};

	/**
	 * Cut out rectangle from rectangle
	 *
	 * \param r  rectangle to cut out
	 *
	 * In the worst case (if we cut a hole into the rectangle) we get
	 * four valid resulting rectangles.
	 */
	Cut_remainder cut(Rect r) const
	{
		/* limit the cut-out area to the actual rectangle */
		r = intersect(r, *this);

		return {
			.top    = compound(Point(x1(), y1()),         Point(x2(), r.y1() - 1)),
			.left   = compound(Point(x1(), r.y1()),       Point(r.x1() - 1, r.y2())),
			.right  = compound(Point(r.x2() + 1, r.y1()), Point(x2(), r.y2())),
			.bottom = compound(Point(x1(), r.y2() + 1),   Point(x2(), y2()))
		};
	}

	/**
	 * Return position of an area when centered within the rectangle
	 */
	Point center(Area const area) const
	{
		return Point((CT(w()) - CT(area.w))/2,
		             (CT(h()) - CT(area.h))/2) + at;
	}

	/**
	 * Print rectangle coordinates
	 *
	 * The output has the form 'width' x 'height' +/- 'p1.x' +/- 'p1.y'.
	 * For example, a rectange of size 15x16 as position (-13, 14) is
	 * printed as "15x16-13+14".
	 */
	void print(Output &out) const { Genode::print(out, area, at); }

	/**
	 * Construct rectangle from XML node attributes
	 *
	 * The XML node is expected to feature the attributes 'xpos', 'ypos'.
	 * 'width', and 'height'. If an attribute is absent, the corresponding
	 * value is set to 0.
	 */
	static Rect from_xml(Xml_node const &node)
	{
		return Rect(Point::from_xml(node), Area::from_xml(node));
	}
};

#endif /* _INCLUDE__UTIL__GEOMETRY_H_ */
