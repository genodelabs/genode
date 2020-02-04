/*
 * \brief  Helper for implementing geometric transitions
 * \author Norman Feske
 * \date   2017-08-17
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__ANIMATED_GEOMETRY_H_
#define _INCLUDE__GEMS__ANIMATED_GEOMETRY_H_

/* demo includes */
#include <util/lazy_value.h>
#include <gems/animator.h>
#include <os/surface.h>

namespace Genode { class Animated_rect; }


class Genode::Animated_rect : private Animator::Item, Noncopyable
{
	public:

		struct Steps { unsigned value; };

		typedef Surface_base::Rect  Rect;
		typedef Surface_base::Area  Area;
		typedef Surface_base::Point Point;

	private:

		Rect _rect { };

		using Lazy_value = ::Lazy_value<int64_t>;

		struct Animated_point
		{
			bool _initial = true;

			Lazy_value _x { }, _y { };

			void animate() { _x.animate(); _y.animate(); }

			bool animated() const { return _x != _x.dst() || _y != _y.dst(); }

			void move_to(Point p, Steps steps)
			{
				if (_initial) {
					_x = Lazy_value(p.x() << 10);
					_y = Lazy_value(p.y() << 10);
					_initial = false;
				} else {
					_x.dst(p.x() << 10, steps.value);
					_y.dst(p.y() << 10, steps.value);
				}
			}

			int x() const { return _x >> 10; }
			int y() const { return _y >> 10; }
		};

		Animated_point _p1 { }, _p2 { };

		Steps _remaining { 0 };

	public:

		Animated_rect(Animator &animator) : Animator::Item(animator) { }

		/**
		 * Animator::Item interface
		 */
		void animate() override
		{
			_p1.animate(); _p2.animate();

			_rect = Rect(Point(_p1.x(), _p1.y()), Point(_p2.x(), _p2.y()));

			if (_remaining.value > 1)
				_remaining.value--;

			/* schedule / de-schedule animation */
			Animator::Item::animated(_p1.animated() || _p2.animated());
		}

		/**
		 * Assign new target coordinates
		 *
		 * The first assignment moves the rectangle directly to the target
		 * position without animation. All subsequent assignments result in an
		 * animated movement.
		 */
		void move_to(Rect rect, Steps steps)
		{
			/* retarget animation while already in progress */
			if (animated())
				steps.value = max(_remaining.value, 1U);

			_remaining = steps;

			_p1.move_to(rect.p1(), steps);
			_p2.move_to(rect.p2(), steps);
			animate();
		}

		bool animated() const { return Animator::Item::animated(); }

		bool initialized() const { return _p1._initial == false; }

		Rect  rect() const { return _rect; }
		Area  area() const { return _rect.area(); }
		Point p1()   const { return _rect.p1(); }
		Point p2()   const { return _rect.p2(); }
};

#endif /* _INCLUDE__GEMS__ANIMATED_GEOMETRY_H_ */
