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

#ifndef _ANIMATED_GEOMETRY_H_
#define _ANIMATED_GEOMETRY_H_

/* demo includes */
#include <util/lazy_value.h>
#include <gems/animator.h>

/* local includes */
#include "types.h"

namespace Menu_view { class Animated_rect; }


class Menu_view::Animated_rect : public Rect, Animator::Item, Noncopyable
{
	public:

		struct Steps { unsigned value; };

	private:

		struct Animated_point
		{
			bool _initial = true;

			Lazy_value<int> _x, _y;

			void animate() { _x.animate(); _y.animate(); }

			bool animated() const { return _x != _x.dst() || _y != _y.dst(); }

			void move_to(Point p, Steps steps)
			{
				if (_initial) {
					_x = Lazy_value<int>(p.x() << 10);
					_y = Lazy_value<int>(p.y() << 10);
					_initial = false;
				} else {
					_x.dst(p.x() << 10, steps.value);
					_y.dst(p.y() << 10, steps.value);
				}
			}

			int x() const { return _x >> 10; }
			int y() const { return _y >> 10; }
		};

		Animated_point _p1, _p2;

	public:

		Animated_rect(Animator &animator) : Animator::Item(animator) { }

		/**
		 * Animator::Item interface
		 */
		void animate() override
		{
			_p1.animate(); _p2.animate();

			static_cast<Rect &>(*this) = Rect(Point(_p1.x(), _p1.y()),
			                                  Point(_p2.x(), _p2.y()));

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
			_p1.move_to(rect.p1(), steps);
			_p2.move_to(rect.p2(), steps);
			animate();
		}

		bool animated() const { return Animator::Item::animated(); }
};

#endif /* _ANIMATED_GEOMETRY_H_ */
